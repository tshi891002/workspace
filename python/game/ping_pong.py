import tkinter as tk

WIDTH = 800
HEIGHT = 450
PADDLE_WIDTH = 10
PADDLE_HEIGHT = 100
BALL_SIZE = 18
SPEED = 6

class PingPong(tk.Frame):
    def __init__(self, master):
        super().__init__(master)
        master.title('Ping Pong')
        self.pack()
        self.canvas = tk.Canvas(self, width=WIDTH, height=HEIGHT, bg='black')
        self.canvas.pack()
        self.left_score = 0
        self.right_score = 0
        self.init_game()
        master.bind('<w>', lambda e: self.move_left(-1))
        master.bind('<s>', lambda e: self.move_left(1))
        master.bind('<Up>', lambda e: self.move_right(-1))
        master.bind('<Down>', lambda e: self.move_right(1))
        master.bind('r', lambda e: self.reset())

    def init_game(self):
        self.left_paddle = self.canvas.create_rectangle(20, HEIGHT//2 - PADDLE_HEIGHT//2,
                                                        20 + PADDLE_WIDTH, HEIGHT//2 + PADDLE_HEIGHT//2,
                                                        fill='white')
        self.right_paddle = self.canvas.create_rectangle(WIDTH - 20 - PADDLE_WIDTH, HEIGHT//2 - PADDLE_HEIGHT//2,
                                                         WIDTH - 20, HEIGHT//2 + PADDLE_HEIGHT//2,
                                                         fill='white')
        self.ball = self.canvas.create_oval(WIDTH//2 - BALL_SIZE//2, HEIGHT//2 - BALL_SIZE//2,
                                            WIDTH//2 + BALL_SIZE//2, HEIGHT//2 + BALL_SIZE//2,
                                            fill='red')
        self.ball_vel = [SPEED, SPEED]
        self.score_text = self.canvas.create_text(WIDTH//2, 20, text='0 : 0', fill='white', font=('Arial', 20))
        self.running = True
        self.animate()

    def move_left(self, direction):
        x1, y1, x2, y2 = self.canvas.coords(self.left_paddle)
        if 0 <= y1 + direction * 20 and y2 + direction * 20 <= HEIGHT:
            self.canvas.move(self.left_paddle, 0, direction * 20)

    def move_right(self, direction):
        x1, y1, x2, y2 = self.canvas.coords(self.right_paddle)
        if 0 <= y1 + direction * 20 and y2 + direction * 20 <= HEIGHT:
            self.canvas.move(self.right_paddle, 0, direction * 20)

    def animate(self):
        if not self.running:
            return
        self.move_ball()
        self.after(20, self.animate)

    def move_ball(self):
        self.canvas.move(self.ball, *self.ball_vel)
        bx1, by1, bx2, by2 = self.canvas.coords(self.ball)
        if by1 <= 0 or by2 >= HEIGHT:
            self.ball_vel[1] = -self.ball_vel[1]
        if self.check_collision(self.left_paddle, bx1, by1, bx2, by2):
            self.ball_vel[0] = abs(self.ball_vel[0])
        if self.check_collision(self.right_paddle, bx1, by1, bx2, by2):
            self.ball_vel[0] = -abs(self.ball_vel[0])
        if bx1 <= 0:
            self.right_score += 1
            self.update_score()
            self.reset_ball(direction=1)
        elif bx2 >= WIDTH:
            self.left_score += 1
            self.update_score()
            self.reset_ball(direction=-1)

    def check_collision(self, paddle, bx1, by1, bx2, by2):
        px1, py1, px2, py2 = self.canvas.coords(paddle)
        return bx1 < px2 and bx2 > px1 and by1 < py2 and by2 > py1

    def update_score(self):
        self.canvas.itemconfigure(self.score_text, text=f'{self.left_score} : {self.right_score}')

    def reset_ball(self, direction=1):
        self.canvas.coords(self.ball, WIDTH//2 - BALL_SIZE//2, HEIGHT//2 - BALL_SIZE//2,
                           WIDTH//2 + BALL_SIZE//2, HEIGHT//2 + BALL_SIZE//2)
        self.ball_vel = [direction * SPEED, SPEED if self.ball_vel[1] >= 0 else -SPEED]

    def reset(self):
        self.left_score = 0
        self.right_score = 0
        self.update_score()
        self.canvas.coords(self.left_paddle, 20, HEIGHT//2 - PADDLE_HEIGHT//2,
                           20 + PADDLE_WIDTH, HEIGHT//2 + PADDLE_HEIGHT//2)
        self.canvas.coords(self.right_paddle, WIDTH - 20 - PADDLE_WIDTH, HEIGHT//2 - PADDLE_HEIGHT//2,
                           WIDTH - 20, HEIGHT//2 + PADDLE_HEIGHT//2)
        self.reset_ball()

if __name__ == '__main__':
    root = tk.Tk()
    PingPong(root)
    root.mainloop()
