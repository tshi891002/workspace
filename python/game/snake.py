# Snake
# -----
# This game models the snake as a list of grid coordinates. That design makes
# movement, collision detection, and growth easy to reason about: the head is
# always the first item in the list, and the body follows behind it.

import random
import tkinter as tk

# The world is divided into an invisible grid. Each snake segment and each food
# item occupies one grid cell, and the Canvas simply draws those cells.
CELL_SIZE = 20
GRID_WIDTH = 30
GRID_HEIGHT = 20
DELAY = 120


class SnakeGame(tk.Frame):
    """Classic snake game implemented with a grid-based coordinate model."""

    def __init__(self, master):
        super().__init__(master)
        master.title("Snake")
        self.pack()

        # The Canvas is the game board. Drawing each cell as a rectangle keeps
        # the game lightweight and easy to understand.
        self.canvas = tk.Canvas(self, width=GRID_WIDTH * CELL_SIZE, height=GRID_HEIGHT * CELL_SIZE, bg="black")
        self.canvas.pack()

        # The score label is separate from the Canvas so it can be updated
        # without redrawing the entire board.
        self.score_label = tk.Label(self, text="Score: 0", font=("Arial", 14))
        self.score_label.pack(pady=4)

        # Store the scheduled callback so it can be cancelled during reset.
        self.after_id = None
        self.reset()

        # Keyboard bindings translate user input into direction changes.
        master.bind('<Up>', lambda e: self.change_direction('Up'))
        master.bind('<Down>', lambda e: self.change_direction('Down'))
        master.bind('<Left>', lambda e: self.change_direction('Left'))
        master.bind('<Right>', lambda e: self.change_direction('Right'))
        master.bind('r', lambda e: self.reset())

    def reset(self):
        # reset() recreates the snake from scratch and sets the score back to
        # zero. This keeps the starting conditions deterministic.
        self.direction = 'Right'
        self.snake = [
            (GRID_WIDTH // 2, GRID_HEIGHT // 2),
            (GRID_WIDTH // 2 - 1, GRID_HEIGHT // 2),
            (GRID_WIDTH // 2 - 2, GRID_HEIGHT // 2),
        ]
        self.generate_food()
        self.game_over = False
        self.score = 0
        self.score_label.config(text="Score: 0")
        self.draw()

        # Cancel any old scheduled step before starting a new loop to prevent
        # multiple timers from piling up during restart.
        if self.after_id:
            self.after_cancel(self.after_id)
        self.after_id = self.after(DELAY, self.step)

    def generate_food(self):
        # The food location is chosen randomly but must not overlap the snake.
        while True:
            self.food = (random.randrange(GRID_WIDTH), random.randrange(GRID_HEIGHT))
            if self.food not in self.snake:
                break

    def change_direction(self, new_dir):
        # The reverse direction is blocked to prevent the snake from instantly
        # turning back on itself.
        opposite = {'Up': 'Down', 'Down': 'Up', 'Left': 'Right', 'Right': 'Left'}
        if not self.game_over and new_dir != opposite.get(self.direction):
            self.direction = new_dir

    def step(self):
        # Each timer tick advances the snake by one cell. The game ends if the
        # new head goes out of bounds or collides with the existing body.
        if self.game_over:
            return

        head_x, head_y = self.snake[0]
        if self.direction == 'Up':
            head_y -= 1
        elif self.direction == 'Down':
            head_y += 1
        elif self.direction == 'Left':
            head_x -= 1
        elif self.direction == 'Right':
            head_x += 1

        new_head = (head_x, head_y)
        if (
            head_x < 0
            or head_x >= GRID_WIDTH
            or head_y < 0
            or head_y >= GRID_HEIGHT
            or new_head in self.snake
        ):
            self.finish()
            return

        # Appending the new head represents movement. If the snake eats food, the
        # tail is not removed so the snake grows by one cell.
        self.snake.insert(0, new_head)
        if new_head == self.food:
            self.score += 1
            self.score_label.config(text=f"Score: {self.score}")
            self.generate_food()
        else:
            self.snake.pop()

        self.draw()
        self.after_id = self.after(DELAY, self.step)

    def finish(self):
        # The game over state stops movement and shows a restart prompt.
        self.game_over = True
        self.canvas.create_text(
            GRID_WIDTH * CELL_SIZE // 2,
            GRID_HEIGHT * CELL_SIZE // 2,
            text="GAME OVER\nPress R to restart",
            fill="white",
            font=("Arial", 20),
            justify='center',
        )

    def draw(self):
        # Redraw the full board each tick. This approach is simple and avoids
        # tracking cell-by-cell changes separately.
        self.canvas.delete('all')
        self.canvas.create_rectangle(0, 0, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE, outline='gray')
        for x, y in self.snake:
            self.draw_cell(x, y, 'lime')
        fx, fy = self.food
        self.draw_cell(fx, fy, 'red')

    def draw_cell(self, x, y, color):
        # Each grid cell is painted as a rectangle so the board can be rendered
        # consistently regardless of the snake's current shape.
        x1, y1 = x * CELL_SIZE, y * CELL_SIZE
        x2, y2 = x1 + CELL_SIZE, y1 + CELL_SIZE
        self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline='darkgreen')


if __name__ == '__main__':
    root = tk.Tk()
    SnakeGame(root)
    root.mainloop()
