import torch
import torch.nn as nn
import torch.optim as optim
import torch.nn.functional as F
from collections import deque
import random
import numpy as np
import os

class Linear_QNet(nn.Module):
    def __init__(self, input_dim: int, hidden_dim: int, output_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, hidden_dim),
            nn.ReLU(inplace=True),
            nn.Linear(hidden_dim, hidden_dim),
            nn.ReLU(inplace=True),
            nn.Linear(hidden_dim, output_dim),
        )

    # 前向传播, 两个线性层中间使用ReLU激活函数
    def forward(self, x):
        return self.net(x)

    def save(self, file_name='model.pth'):
        model_folder_path = './model'
        if not os.path.exists(model_folder_path):
            os.makedirs(model_folder_path)

        file_name = os.path.join(model_folder_path, file_name)
        torch.save(self.state_dict(), file_name)

    def save_pt(self, file_name='model.pt'):
        model_folder_path = './model'
        if not os.path.exists(model_folder_path):
            os.makedirs(model_folder_path)

        file_name = os.path.join(model_folder_path, file_name)
        # 使用 torch.jit.trace() 导出 TorchScript 格式，RKNN 工具需要这种格式
        self.eval()
        dummy_input = torch.randn(1, self.net[0].in_features)
        scripted_model = torch.jit.trace(self, dummy_input)
        scripted_model.save(file_name)

    def load(self, file_name='model.pth'):
        model_folder_path = './model'
        file_name = os.path.join(model_folder_path, file_name)
        self.load_state_dict(torch.load(file_name))

    def load_pt(self, file_name='model.pt'):
        model_folder_path = './model'
        file_name = os.path.join(model_folder_path, file_name)
        self.load_state_dict(torch.load(file_name, weights_only=False).state_dict())

    def export_onnx(self, onnx_path='model.onnx', opset_version=13):
        # 导出当前模型为 ONNX，输入 shape: [batch, input_size]，输出 shape: [batch, n_actions]
        self.eval()
        dummy = torch.randn(1, self.linear1.in_features, dtype=torch.float32)
        torch.onnx.export(
            self, dummy, onnx_path,
            input_names=['state'],
            output_names=['q_values'],
            dynamic_axes={'state': {0: 'batch'}, 'q_values': {0: 'batch'}},
            opset_version=opset_version
        )


class QTrainer:
    def __init__(self, lr, gamma,input_dim, hidden_dim, output_dim):
        self.gamma = gamma # 折扣因子, 用于计算未来奖励
        self.hidden_size = hidden_dim # 隐藏层大小
        self.model = Linear_QNet(input_dim, self.hidden_size, output_dim) # Q网络模型
        self.target_model = Linear_QNet(input_dim, self.hidden_size,output_dim) # 目标网络模型
        self.optimizer = optim.Adam(self.model.parameters(), lr=lr) # 优化器
        self.criterion = nn.MSELoss() # 损失函数
        self.copy_model() # 初始化时复制模型参数

    def copy_model(self):
        self.target_model.load_state_dict(self.model.state_dict())

    def train_step(self, state, action, reward, next_state, done):
        # 转换一下数据格式
        state = torch.tensor(state, dtype=torch.float)
        next_state = torch.tensor(next_state, dtype=torch.float)
        action = torch.tensor(action, dtype=torch.long)
        action = torch.unsqueeze(action, -1)
        reward = torch.tensor(reward, dtype=torch.float)
        done = torch.tensor(done, dtype=torch.long)
        # 计算当前Q值和目标Q值
        # 
        Q_value = self.model(state).gather(-1, action).squeeze()
        Q_value_next = self.target_model(next_state).detach().max(-1)[0]
        target =  (reward + self.gamma * Q_value_next * (1 - done)).squeeze()

        self.optimizer.zero_grad()
        loss = self.criterion(Q_value,target)
        loss.backward()
        self.optimizer.step()

    def export_onnx(self, onnx_path='model.onnx', opset_version=17):
        # 使用新导出器优先导出到高 opset，避免降级转换
        self.eval()
        dummy = torch.randn(1, self.linear1.in_features, dtype=torch.float32)
        try:
            import torch.onnx
            if hasattr(torch.onnx, "dynamo_export"):
                export_opts = torch.onnx.ExportOptions(opset_version=opset_version)
                torch.onnx.dynamo_export(self, dummy, export_options=export_opts).save(onnx_path)
            else:
                torch.onnx.export(
                    self, dummy, onnx_path,
                    input_names=['state'], output_names=['q_values'],
                    dynamic_axes={'state': {0: 'batch'}, 'q_values': {0: 'batch'}},
                    opset_version=opset_version
                )
        except Exception as e:
            raise RuntimeError(f"ONNX 导出失败: {e}")

class Agent:
    def __init__(self,nS,nA,max_explore=100, gamma = 0.9,
                max_memory=5000, lr=0.001, hidden_dim=128):
        self.max_explore = max_explore 
        self.memory = deque(maxlen=max_memory) 
        self.nS = nS
        self.nA = nA
        self.n_game = 0
        self.trainer = QTrainer(lr, gamma, self.nS, hidden_dim, self.nA)

    def remember(self, state, action, reward, next_state, done):
        self.memory.append((state, action, reward, next_state, done)) 

    def train_long_memory(self,batch_size):
        if len(self.memory) > batch_size:
            mini_sample = random.sample(self.memory, batch_size) # list of tuples
        else:
            mini_sample = self.memory
        states, actions, rewards, next_states, dones = zip(*mini_sample)
        states = np.array(states)
        next_states = np.array(next_states)
        self.trainer.train_step(states, actions, rewards, next_states, dones)

    def train_short_memory(self, state, action, reward, next_state, done):
        self.trainer.train_step(state, action, reward, next_state, done)


    def get_action(self, state, n_game, explore=True):
        state = torch.tensor(state, dtype=torch.float)
        prediction = self.trainer.model(state).detach().numpy().squeeze()
        epsilon = self.max_explore - n_game
        if explore and random.randint(0, self.max_explore) < epsilon:
            prob = np.exp(prediction)/np.exp(prediction).sum()
            final_move = np.random.choice(len(prob), p=prob)
        else:
            final_move = prediction.argmax()
        return final_move

def KerasSinNet(input_dim: int, hidden_dim: int, output_dim: int):
    from tensorflow import keras
    from tensorflow.keras import layers

    model = keras.Sequential([
        layers.InputLayer(input_shape=(input_dim,)),
        layers.Dense(hidden_dim, activation='relu'),
        layers.Dense(hidden_dim, activation='relu'),
        layers.Dense(output_dim, activation=None),  # 线性输出与 PyTorch 一致
    ])
    return model


class PytorchToKeras(object):
    def __init__(self, pModel: nn.Module, kModel):
        super(PytorchToKeras, self).__init__()
        self.__source_layers = []
        self.__target_layers = []
        self.pModel = pModel.eval()
        self.kModel = kModel

        # 兼容 TF2：关闭训练态以便设置权重
        try:
            import tensorflow as tf
            tf.keras.backend.set_learning_phase(False)
        except Exception:
            pass

    def __retrieve_k_layers(self):
        # 收集 Keras 中含权重的层索引（Dense、Conv、BN等）
        self.__target_layers = []
        for i, layer in enumerate(self.kModel.layers):
            if len(layer.weights) > 0:
                self.__target_layers.append(i)

    def __retrieve_p_layers(self, input_size: int):
        # 通过 forward hook 收集 PyTorch 中的有权重的层（Linear/Conv/BN）
        self.__source_layers = []
        dummy = torch.randn(input_size)
        dummy = dummy.unsqueeze(0)  # 增加 batch 维

        hooks = []
        def add_hooks(module):
            def hook(module, input, output):
                if hasattr(module, "weight"):
                    self.__source_layers.append(module)
            # 跳过容器模块，只在叶子模块挂钩
            if not isinstance(module, nn.ModuleList) and not isinstance(module, nn.Sequential) and module is not self.pModel:
                hooks.append(module.register_forward_hook(hook))

        self.pModel.apply(add_hooks)
        with torch.no_grad():
            self.pModel(dummy)
        for h in hooks:
            h.remove()

    def convert(self, input_size: int):
        self.__retrieve_k_layers()
        self.__retrieve_p_layers(input_size)

        # 对齐 PyTorch 层与 Keras 层数量
        if len(self.__source_layers) != len(self.__target_layers):
            raise RuntimeError(f"层数量不匹配: pytorch={len(self.__source_layers)} keras={len(self.__target_layers)}")

        for source_layer, target_idx in zip(self.__source_layers, self.__target_layers):
            # 根据层类型转换权重形状
            if isinstance(source_layer, nn.Conv2d):
                # PyTorch: [out_c, in_c, kH, kW] -> Keras: [kH, kW, in_c, out_c]
                w = source_layer.weight.detach().cpu().numpy().transpose(2, 3, 1, 0)
                b = source_layer.bias.detach().cpu().numpy() if source_layer.bias is not None else None
                if b is not None:
                    self.kModel.layers[target_idx].set_weights([w, b])
                else:
                    self.kModel.layers[target_idx].set_weights([w])
            elif isinstance(source_layer, nn.BatchNorm2d):
                gamma = source_layer.weight.detach().cpu().numpy()
                beta = source_layer.bias.detach().cpu().numpy()
                mean = source_layer.running_mean.detach().cpu().numpy()
                var = source_layer.running_var.detach().cpu().numpy()
                self.kModel.layers[target_idx].set_weights([gamma, beta, mean, var])
            elif isinstance(source_layer, nn.Linear):
                # PyTorch: [out_features, in_features] -> Keras: [in_features, out_features]
                w = source_layer.weight.detach().cpu().numpy().T
                b = source_layer.bias.detach().cpu().numpy() if source_layer.bias is not None else None
                if b is not None:
                    self.kModel.layers[target_idx].set_weights([w, b])
                else:
                    self.kModel.layers[target_idx].set_weights([w])
            else:
                # 其他类型暂不支持
                raise NotImplementedError(f"不支持的层类型: {type(source_layer)}")

    def save_model(self, output_file: str):
        self.kModel.save(output_file)

    def save_weights(self, output_file: str):
        self.kModel.save_weights(output_file, save_format='h5')



def export_tflite_int8_from_ckpt(ckpt_path: str,
                                 tflite_path: str,
                                 input_dim: int,
                                 hidden_dim: int,
                                 output_dim: int,
                                 representative_data: "Iterable[np.ndarray]"):
    """
    从 PyTorch 权重导出全整型(INT8) TFLite 模型。需要代表性数据用于校准。
    representative_data: 可迭代对象，产生形状为 [input_dim] 或 [1, input_dim] 的 float32 numpy 数组。
    """
    import tensorflow as tf

    # 1) 加载 PyTorch 权重
    device = torch.device("cpu")
    pt_model = Linear_QNet(input_dim, hidden_dim, output_dim).to(device)
    state = torch.load(ckpt_path, map_location=device)
    if isinstance(state, dict) and "state_dict" in state:
        pt_model.load_state_dict(state["state_dict"])
    elif isinstance(state, dict):
        pt_model.load_state_dict(state)
    else:
        pt_model = state.to(device)
    pt_model.eval()

    # 2) 构建等价 Keras 模型并拷贝权重
    keras_model = KerasSinNet(input_dim, hidden_dim, output_dim)
    PytorchToKeras(pt_model, keras_model).convert(input_dim)

    # 3) 代表性数据生成器（用于激活的量化校准）
    def rep_dataset():
        for x in representative_data:
            x = np.asarray(x, dtype=np.float32).reshape(1, input_dim)
            yield [x]

    # 4) 全整型量化（确保输入/输出也是 int8）
    converter = tf.lite.TFLiteConverter.from_keras_model(keras_model)
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = rep_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8
    tflite_model = converter.convert()

    # 5) 保存 .tflite
    os.makedirs(os.path.dirname(tflite_path) or ".", exist_ok=True)
    with open(tflite_path, "wb") as f:
        f.write(tflite_model)

    # 6) 额外导出 C 头文件，便于 TFLM 集成
    try:
        h_path = os.path.splitext(tflite_path)[0] + ".h"
        var = os.path.splitext(os.path.basename(tflite_path))[0]
        with open(h_path, "w", encoding="utf-8") as f:
            arr = ",".join(str(b) for b in tflite_model)
            f.write(f"const unsigned char {var}[] = {{{arr}}};\n")
            f.write(f"const int {var}_len = {len(tflite_model)};\n")
        print(f"已导出: {tflite_path} 和 {h_path}")
    except Exception as e:
        print(f"写入头文件失败（忽略）：{e}")