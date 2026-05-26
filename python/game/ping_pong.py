# Ping Pong
# ---------
# This script uses Tkinter's Canvas widget to draw a simple 2D game.
# The core design idea is to keep all gameplay state in a few attributes on
# the PingPong class and to update the visual objects on every animation tick.
# Instead of a full physics engine, the game uses small constant steps so the
# ball movement stays predictable and easy to understand.

import tkinter as tk

# Board dimensions are kept as constants so they can be adjusted without
# changing the movement logic scattered across the file.
WIDTH = 800
HEIGHT = 450
PADDLE_WIDTH = 10
PADDLE_HEIGHT = 100
BALL_SIZE = 18
SPEED = 6


class PingPong(tk.Frame):
    """Simple table-tennis style game built on Tkinter's Canvas."""

    def __init__(self, master):
        # The Frame acts as the game container, so the Canvas and labels can be
        # packed into it cleanly.
        super().__init__(master)
        master.title('Ping Pong')
        self.pack()

        # The Canvas stores every drawable game object, including paddles and
        # the ball. Drawing is done by manipulating Canvas items instead of
        # creating a pixel-based game loop.
        self.canvas = tk.Canvas(self, width=WIDTH, height=HEIGHT, bg='black')
        self.canvas.pack()

        # Score state is stored explicitly so the text overlay can be updated
        # whenever a point is awarded.
        self.left_score = 0
        self.right_score = 0

        # Initialize the playing field and begin the animation loop.
        self.init_game()

        # Keyboard handlers are bound directly to the root window, which keeps
        # the game logic in one place and avoids extra widgets for controls.
        master.bind('<w>', lambda e: self.move_left(-1))
        master.bind('<s>', lambda e: self.move_left(1))
        master.bind('<Up>', lambda e: self.move_right(-1))
        master.bind('<Down>', lambda e: self.move_right(1))
        master.bind('r', lambda e: self.reset())

    def init_game(self):
        # The paddles are created once and then moved by changing their Canvas
        # coordinates. This is efficient and matches the "object on screen"
        # model of the game.
        self.left_paddle = self.canvas.create_rectangle(
            20,
            HEIGHT // 2 - PADDLE_HEIGHT // 2,
            20 + PADDLE_WIDTH,
            HEIGHT // 2 + PADDLE_HEIGHT // 2,
            fill='white',
        )
        self.right_paddle = self.canvas.create_rectangle(
            WIDTH - 20 - PADDLE_WIDTH,
            HEIGHT // 2 - PADDLE_HEIGHT // 2,
            WIDTH - 20,
            HEIGHT // 2 + PADDLE_HEIGHT // 2,
            fill='white',
        )

        # The ball is a round Canvas item. Its position is updated by moving it
        # by a small velocity vector each animation step.
        self.ball = self.canvas.create_oval(
            WIDTH // 2 - BALL_SIZE // 2,
            HEIGHT // 2 - BALL_SIZE // 2,
            WIDTH // 2 + BALL_SIZE // 2,
            HEIGHT // 2 + BALL_SIZE // 2,
            fill='red',
        )

        # Horizontal and vertical velocities are stored as a list so the code
        # can flip signs easily when the ball bounces or a point is scored.
        self.ball_vel = [SPEED, SPEED]

        # The score text is another Canvas item, making it simple to rewrite the
        # score without rebuilding the UI.
        self.score_text = self.canvas.create_text(
            WIDTH // 2,
            20,
            text='0 : 0',
            fill='white',
            font=('Arial', 20),
        )

        # A simple running flag allows the animation loop to stop cleanly if
        # needed, even though this game does not currently use it for a pause.
        self.running = True
        self.animate()

    def move_left(self, direction):
        # The left paddle is moved in fixed increments, and the boundary check
        # ensures it stays inside the visible play area.
        _, y1, _, y2 = self.canvas.coords(self.left_paddle)
        if 0 <= y1 + direction * 20 and y2 + direction * 20 <= HEIGHT:
            self.canvas.move(self.left_paddle, 0, direction * 20)

    def move_right(self, direction):
        # The right paddle uses the same movement model as the left paddle so
        # both controls feel consistent.
        _, y1, _, y2 = self.canvas.coords(self.right_paddle)
        if 0 <= y1 + direction * 20 and y2 + direction * 20 <= HEIGHT:
            self.canvas.move(self.right_paddle, 0, direction * 20)

    def animate(self):
        # This is the game loop. Every 20 ms, the ball is advanced and the
        # current game state is re-evaluated.
        if not self.running:
            return
        self.move_ball()
        self.after(20, self.animate)

    def move_ball(self):
        # Move the ball by its current velocity vector.
        self.canvas.move(self.ball, *self.ball_vel)
        bx1, by1, bx2, by2 = self.canvas.coords(self.ball)

        # Bounce off the top and bottom edges by negating the vertical
        # component of the velocity.
        if by1 <= 0 or by2 >= HEIGHT:
            self.ball_vel[1] = -self.ball_vel[1]

        # Collision detection uses bounding-box overlap. This is small and
        # readable, which is ideal for a beginner-friendly game.
        if self.check_collision(self.left_paddle, bx1, by1, bx2, by2):
            self.ball_vel[0] = abs(self.ball_vel[0])
        if self.check_collision(self.right_paddle, bx1, by1, bx2, by2):
            self.ball_vel[0] = -abs(self.ball_vel[0])

        # If the ball crosses the left or right edge, the opposite player gets a
        # point and the ball is sent back toward the player who just lost it.
        if bx1 <= 0:
            self.right_score += 1
            self.update_score()
            self.reset_ball(direction=1)
        elif bx2 >= WIDTH:
            self.left_score += 1
            self.update_score()
            self.reset_ball(direction=-1)

    def check_collision(self, paddle, bx1, by1, bx2, by2):
        # The Canvas returns the bounding box of each item as
        # (x1, y1, x2, y2). Overlap is detected when the rectangles intersect.
        px1, py1, px2, py2 = self.canvas.coords(paddle)
        return bx1 < px2 and bx2 > px1 and by1 < py2 and by2 > py1

    def update_score(self):
        # Update the visible score text in place rather than recreating the
        # text item each time.
        self.canvas.itemconfigure(self.score_text, text=f'{self.left_score} : {self.right_score}')

    def reset_ball(self, direction=1):
        # Resetting the ball puts it back in the center and reuses the current
        # vertical direction if possible so the game feels continuous.
        self.canvas.coords(
            self.ball,
            WIDTH // 2 - BALL_SIZE // 2,
            HEIGHT // 2 - BALL_SIZE // 2,
            WIDTH // 2 + BALL_SIZE // 2,
            HEIGHT // 2 + BALL_SIZE // 2,
        )
        self.ball_vel = [direction * SPEED, SPEED if self.ball_vel[1] >= 0 else -SPEED]

    def reset(self):
        # reset() returns the game to a neutral state: scores cleared, paddles
        # moved to their starting positions, and the ball centered.
        self.left_score = 0
        self.right_score = 0
        self.update_score()
        self.canvas.coords(
            self.left_paddle,
            20,
            HEIGHT // 2 - PADDLE_HEIGHT // 2,
            20 + PADDLE_WIDTH,
            HEIGHT // 2 + PADDLE_HEIGHT // 2,
        )
        self.canvas.coords(
            self.right_paddle,
            WIDTH - 20 - PADDLE_WIDTH,
            HEIGHT // 2 - PADDLE_HEIGHT // 2,
            WIDTH - 20,
            HEIGHT // 2 + PADDLE_HEIGHT // 2,
        )
        self.reset_ball()


if __name__ == '__main__':
    root = tk.Tk()
    PingPong(root)
    root.mainloop()
