import tkinter as tk
from tkinter import messagebox

class TicTacToe(tk.Frame):
    def __init__(self, master):
        super().__init__(master)
        master.title("Tic-Tac-Toe")
        self.pack()
        self.current_player = 'X'
        self.board = [None] * 9
        self.buttons = []
        self.create_widgets()

    def create_widgets(self):
        frame = tk.Frame(self)
        frame.pack(padx=10, pady=10)
        for i in range(9):
            button = tk.Button(frame, text='', font=('Arial', 24), width=4, height=2,
                               command=lambda i=i: self.on_click(i))
            button.grid(row=i // 3, column=i % 3)
            self.buttons.append(button)
        self.status_label = tk.Label(self, text="Player X's turn", font=('Arial', 14))
        self.status_label.pack(pady=8)
        tk.Button(self, text='Restart', font=('Arial', 12), command=self.reset).pack(pady=4)

    def on_click(self, index):
        if self.board[index] is None:
            self.board[index] = self.current_player
            self.buttons[index].config(text=self.current_player, disabledforeground='black')
            self.buttons[index].config(state='disabled')
            if self.check_win(self.current_player):
                self.end_game(f'Player {self.current_player} wins!')
            elif all(self.board):
                self.end_game('Draw!')
            else:
                self.current_player = 'O' if self.current_player == 'X' else 'X'
                self.status_label.config(text=f"Player {self.current_player}'s turn")

    def check_win(self, player):
        lines = [
            (0, 1, 2), (3, 4, 5), (6, 7, 8),
            (0, 3, 6), (1, 4, 7), (2, 5, 8),
            (0, 4, 8), (2, 4, 6)
        ]
        return any(all(self.board[pos] == player for pos in line) for line in lines)

    def end_game(self, message):
        self.status_label.config(text=message)
        for button in self.buttons:
            button.config(state='disabled')
        messagebox.showinfo('Game Over', message)

    def reset(self):
        self.current_player = 'X'
        self.board = [None] * 9
        for button in self.buttons:
            button.config(text='', state='normal')
        self.status_label.config(text="Player X's turn")

if __name__ == '__main__':
    root = tk.Tk()
    TicTacToe(root)
    root.mainloop()
