import random
import tkinter as tk

CELL_SIZE = 20
GRID_WIDTH = 30
GRID_HEIGHT = 20
DELAY = 120

class SnakeGame(tk.Frame):
    def __init__(self, master):
        super().__init__(master)
        master.title("Snake")
        self.pack()
        self.canvas = tk.Canvas(self, width=GRID_WIDTH * CELL_SIZE, height=GRID_HEIGHT * CELL_SIZE, bg="black")
        self.canvas.pack()
        self.score_label = tk.Label(self, text="Score: 0", font=("Arial", 14))
        self.score_label.pack(pady=4)
        self.after_id = None
        self.reset()
        master.bind('<Up>', lambda e: self.change_direction('Up'))
        master.bind('<Down>', lambda e: self.change_direction('Down'))
        master.bind('<Left>', lambda e: self.change_direction('Left'))
        master.bind('<Right>', lambda e: self.change_direction('Right'))
        master.bind('r', lambda e: self.reset())

    def reset(self):
        self.direction = 'Right'
        self.snake = [(GRID_WIDTH // 2, GRID_HEIGHT // 2),
                      (GRID_WIDTH // 2 - 1, GRID_HEIGHT // 2),
                      (GRID_WIDTH // 2 - 2, GRID_HEIGHT // 2)]
        self.generate_food()
        self.game_over = False
        self.score = 0
        self.score_label.config(text="Score: 0")
        self.draw()
        if self.after_id:
            self.after_cancel(self.after_id)
        self.after_id = self.after(DELAY, self.step)

    def generate_food(self):
        while True:
            self.food = (random.randrange(GRID_WIDTH), random.randrange(GRID_HEIGHT))
            if self.food not in self.snake:
                break

    def change_direction(self, new_dir):
        opposite = {'Up': 'Down', 'Down': 'Up', 'Left': 'Right', 'Right': 'Left'}
        if not self.game_over and new_dir != opposite.get(self.direction):
            self.direction = new_dir

    def step(self):
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
        if (head_x < 0 or head_x >= GRID_WIDTH or head_y < 0 or head_y >= GRID_HEIGHT or new_head in self.snake):
            self.finish()
            return

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
        self.game_over = True
        self.canvas.create_text(GRID_WIDTH * CELL_SIZE // 2,
                                 GRID_HEIGHT * CELL_SIZE // 2,
                                 text="GAME OVER\nPress R to restart",
                                 fill="white",
                                 font=("Arial", 20),
                                 justify='center')

    def draw(self):
        self.canvas.delete('all')
        self.canvas.create_rectangle(0, 0, GRID_WIDTH * CELL_SIZE, GRID_HEIGHT * CELL_SIZE, outline='gray')
        for x, y in self.snake:
            self.draw_cell(x, y, 'lime')
        fx, fy = self.food
        self.draw_cell(fx, fy, 'red')

    def draw_cell(self, x, y, color):
        x1, y1 = x * CELL_SIZE, y * CELL_SIZE
        x2, y2 = x1 + CELL_SIZE, y1 + CELL_SIZE
        self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline='darkgreen')

if __name__ == '__main__':
    root = tk.Tk()
    SnakeGame(root)
    root.mainloop()
