import numpy as np
from snake_env import Game
from snake_agent import Agent
from helper import plot
import torch

def train():
    plot_scores = []
    plot_mean_scores = []
    record = 0
    total_step = 0
    game = Game()
    agent = Agent(game.nS,game.nA, hidden_dim=128)
    state_new = game.get_state()

    while True:
        state_old = state_new
        final_move = agent.get_action(state_old,agent.n_game)
        reward, done, score = game.play_step(final_move)
        state_new = game.get_state()
        agent.remember(state_old, final_move, reward, state_new, done)
        agent.train_long_memory(batch_size=256)
        total_step += 1

        if total_step % 10 == 0:
            agent.trainer.copy_model()

        if done:
            game.reset()
            agent.n_game += 1
            if score > record:
                record = score
                agent.trainer.model.save()
                print('Saved Model')
            print('Game', agent.n_game, 'Score', score, 'Record:', record)
            plot_scores.append(score)
            mean_scores = np.mean(plot_scores[-10:])
            plot_mean_scores.append(mean_scores)
            plot(plot_scores, plot_mean_scores)

def play():
    record = 0
    total_step = 0
    game = Game()
    agent = Agent(game.nS,game.nA, hidden_dim=128)
    agent.trainer.model.load_pt()
    state_new = game.get_state()

    # 记录前30轮的状态与输出
    max_dump = 30
    dump_states = []
    dump_outputs = []  # 模型Q值输出

    while True:
        state_old = state_new
        q_values = agent.trainer.model(torch.tensor(state_old, dtype=torch.float)).detach().numpy().squeeze()
        final_move = agent.get_action(state_old,agent.n_game)

        # 收集前30轮
        if len(dump_states) < max_dump:
            dump_states.append([float(v) for v in state_old])
            dump_outputs.append([float(q) for q in np.atleast_1d(q_values)])
        
        reward, done, score = game.play_step(final_move)
        state_new = game.get_state()
        total_step += 1

        # 当收集到30条后一次性以C数组形式输出
        if len(dump_states) == max_dump and len(dump_outputs) == max_dump:
            # 打印 states[30][nS]
            print(f"// dump first {max_dump} states as C array")
            print(f"float states[{max_dump}][{game.nS}] = {{")
            for row in dump_states:
                print("    {" + ", ".join(f"{v:.6f}" for v in row) + "},")
            print("};")
            # 打印 outputs[30][nA]
            print(f"// dump first {max_dump} model outputs (Q values) as C array")
            print(f"float outputs[{max_dump}][{game.nA}] = {{")
            for row in dump_outputs:
                print("    {" + ", ".join(f"{v:.6f}" for v in row) + "},")
            print("};")
            # 只打印一次，清空使不再重复
            dump_states.clear()
            dump_outputs.clear()

        if done:
            game.reset()
            agent.n_game += 1
            if score > record:
                record = score
            print('Game', agent.n_game, 'Score', score, 'Record:', record)

def get_pt_model():
    game = Game()
    agent = Agent(game.nS,game.nA, hidden_dim=128)
    agent.trainer.model.load()
    agent.trainer.model.save_pt()
    

if __name__ == '__main__':
    # train()
    # play()

    get_pt_model()