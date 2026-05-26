# Tic-Tac-Toe
# ------------
# This game uses a simple 1D list to represent the 3x3 board. The design
# keeps the state compact and makes win checking easy because every winning line
# can be described as a tuple of board indices.

import tkinter as tk
from tkinter import messagebox


class TicTacToe(tk.Frame):
    """A small turn-based board game implemented with Tkinter buttons."""

    def __init__(self, master):
        super().__init__(master)
        master.title("Tic-Tac-Toe")
        self.pack()

        # current_player tracks whose turn it is, and board stores the marks.
        self.current_player = 'X'
        self.board = [None] * 9
        self.buttons = []
        self.create_widgets()

    def create_widgets(self):
        # A nested frame keeps the game buttons grouped and centered.
        frame = tk.Frame(self)
        frame.pack(padx=10, pady=10)

        # Each button is tied to a board index. Using a lambda with the index
        # makes the click handler know which square the player selected.
        for i in range(9):
            button = tk.Button(
                frame,
                text='',
                font=('Arial', 24),
                width=4,
                height=2,
                command=lambda i=i: self.on_click(i),
            )
            button.grid(row=i // 3, column=i % 3)
            self.buttons.append(button)

        # A status label provides player feedback and game result text.
        self.status_label = tk.Label(self, text="Player X's turn", font=('Arial', 14))
        self.status_label.pack(pady=8)
        tk.Button(self, text='Restart', font=('Arial', 12), command=self.reset).pack(pady=4)

    def on_click(self, index):
        # Only empty squares can be played. Once a move is made, the button is
        # disabled so the same square cannot be chosen twice.
        if self.board[index] is None:
            self.board[index] = self.current_player
            self.buttons[index].config(text=self.current_player, disabledforeground='black')
            self.buttons[index].config(state='disabled')

            if self.check_win(self.current_player):
                self.end_game(f'Player {self.current_player} wins!')
            elif all(self.board):
                self.end_game('Draw!')
            else:
                # Advance the turn after a valid move.
                self.current_player = 'O' if self.current_player == 'X' else 'X'
                self.status_label.config(text=f"Player {self.current_player}'s turn")

    def check_win(self, player):
        # Winning lines are stored as index tuples. This keeps the logic easy to
        # update and avoids hardcoding conditionals for each possible pattern.
        lines = [
            (0, 1, 2),
            (3, 4, 5),
            (6, 7, 8),
            (0, 3, 6),
            (1, 4, 7),
            (2, 5, 8),
            (0, 4, 8),
            (2, 4, 6),
        ]
        return any(all(self.board[pos] == player for pos in line) for line in lines)

    def end_game(self, message):
        # Once the game is over, all moves are locked and the player is informed
        # via a popup dialog.
        self.status_label.config(text=message)
        for button in self.buttons:
            button.config(state='disabled')
        messagebox.showinfo('Game Over', message)

    def reset(self):
        # reset() clears the board and restores the initial turn.
        self.current_player = 'X'
        self.board = [None] * 9
        for button in self.buttons:
            button.config(text='', state='normal')
        self.status_label.config(text="Player X's turn")


if __name__ == '__main__':
    root = tk.Tk()
    TicTacToe(root)
    root.mainloop()
