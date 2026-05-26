# Flying Shooting
# ---------------
# This game uses Tkinter's Canvas like a lightweight sprite engine.
# The design pattern is simple: keep the ship, bullets, and enemies as Canvas
# items, store their IDs in lists, and run a periodic animation callback that
# updates positions and checks for collisions.

import random
import tkinter as tk

# Screen and object sizes are global constants so the game behavior stays
# consistent while remaining easy to tweak.
WIDTH = 600
HEIGHT = 480
SHIP_SIZE = 40
BULLET_SIZE = 6
ENEMY_SIZE = 30
ENEMY_COUNT = 6


class ShootingGame(tk.Frame):
    """A top-down shooting game with a simple scoring loop."""

    def __init__(self, master):
        super().__init__(master)
        master.title('Flying Shooting')
        self.pack()

        # The canvas is the drawing surface for the ship, bullets, and enemies.
        self.canvas = tk.Canvas(self, width=WIDTH, height=HEIGHT, bg='navy')
        self.canvas.pack()

        # Game state is kept as simple flags and counters.
        self.score = 0
        self.game_over = False

        # These lists hold the Canvas IDs of active objects. This avoids
        # storing full object wrappers and keeps the update code straightforward.
        self.ship = self.canvas.create_polygon(0, 0, 0, 0, 0, 0, fill='cyan')
        self.bullets = []
        self.enemies = []

        self.create_ui()

        # Keyboard bindings provide the player controls and the restart action.
        master.bind('<Left>', lambda e: self.move_ship(-20))
        master.bind('<Right>', lambda e: self.move_ship(20))
        master.bind('<space>', lambda e: self.fire())
        master.bind('r', lambda e: self.reset())

        # The reset method builds the initial scene and draw state.
        self.reset()
        self.animate()

    def create_ui(self):
        # The score label is a regular Tk label placed on top of the Canvas.
        # Placing it separately keeps the Canvas free to render the game world.
        self.score_label = tk.Label(self, text='Score: 0', fg='white', bg='navy', font=('Arial', 14))
        self.score_label.place(x=10, y=10)

    def reset(self):
        # reset() restores the game to its starting state so the player can
        # replay the round without restarting the application.
        self.game_over = False
        self.score = 0
        self.update_score()

        # Redraw the background and clear previous objects.
        self.canvas.delete('all')
        self.canvas.config(bg='navy')
        self.create_ui()
        self.bullets = []
        self.enemies = []

        # The ship position is stored independently from the Canvas item so the
        # movement code can clamp it before writing new coordinates.
        self.ship_x = WIDTH // 2
        self.ship_y = HEIGHT - 60
        self.ship = self.canvas.create_polygon(0, 0, 0, 0, 0, 0, fill='cyan')
        self.draw_ship()

        # Enemies are randomly spawned above the playfield and then drift down
        # toward the player.
        for _ in range(ENEMY_COUNT):
            x = random.randint(50, WIDTH - ENEMY_SIZE - 50)
            y = random.randint(-300, -50)
            enemy = self.canvas.create_rectangle(x, y, x + ENEMY_SIZE, y + ENEMY_SIZE, fill='orange')
            self.enemies.append(enemy)

    def draw_ship(self):
        # The ship is represented as a triangle. Its coordinates are rebuilt on
        # every movement update so the shape stays centered on the ship_x value.
        coords = [
            self.ship_x,
            self.ship_y,
            self.ship_x - SHIP_SIZE // 2,
            self.ship_y + SHIP_SIZE,
            self.ship_x + SHIP_SIZE // 2,
            self.ship_y + SHIP_SIZE,
        ]
        self.canvas.coords(self.ship, *coords)

    def move_ship(self, dx):
        # The ship cannot leave the visible board, so the new position is
        # clamped between the left and right edges.
        if self.game_over:
            return
        new_x = min(max(self.ship_x + dx, SHIP_SIZE // 2), WIDTH - SHIP_SIZE // 2)
        self.ship_x = new_x
        self.draw_ship()

    def fire(self):
        # Bullets are created at the ship's current position and travel upward.
        if self.game_over:
            return
        bullet = self.canvas.create_rectangle(
            self.ship_x - BULLET_SIZE // 2,
            self.ship_y - 20,
            self.ship_x + BULLET_SIZE // 2,
            self.ship_y - 10,
            fill='yellow',
        )
        self.bullets.append(bullet)

    def animate(self):
        # The animation loop runs every 40 ms. Each pass updates bullets,
        # enemies, and then checks whether anything important happened.
        if self.game_over:
            return
        self.move_bullets()
        self.move_enemies()
        self.check_collisions()
        self.after(40, self.animate)

    def move_bullets(self):
        # Bullets are moved upward each tick, and removed once they leave the
        # top of the screen to avoid leaking Canvas items.
        for bullet in self.bullets[:]:
            self.canvas.move(bullet, 0, -12)
            _, _, _, y2 = self.canvas.coords(bullet)
            if y2 < 0:
                self.canvas.delete(bullet)
                self.bullets.remove(bullet)

    def move_enemies(self):
        # Enemies drift downward continuously. When one leaves the playfield,
        # it is respawned above the screen so the challenge never ends.
        for enemy in self.enemies:
            self.canvas.move(enemy, 0, 4)
            x1, y1, _, _ = self.canvas.coords(enemy)
            if y1 > HEIGHT:
                x = random.randint(50, WIDTH - ENEMY_SIZE - 50)
                y = random.randint(-300, -50)
                self.canvas.coords(enemy, x, y, x + ENEMY_SIZE, y + ENEMY_SIZE)

    def check_collisions(self):
        # Collision checking is done in two passes:
        # 1. bullets vs enemies (score increment and spawn replacement)
        # 2. enemies vs ship (game over)
        for enemy in self.enemies[:]:
            for bullet in self.bullets[:]:
                if self.overlaps(enemy, bullet):
                    self.canvas.delete(enemy)
                    self.canvas.delete(bullet)
                    self.enemies.remove(enemy)
                    self.bullets.remove(bullet)
                    self.score += 10
                    self.update_score()
                    self.spawn_enemy()
                    break

            if self.overlaps(enemy, self.ship):
                self.end_game()
                return

    def overlaps(self, item1, item2):
        # bbox() returns the bounding box of each Canvas item. This helper keeps
        # the collision logic readable by hiding the low-level rectangle math.
        bbox1 = self.canvas.bbox(item1)
        bbox2 = self.canvas.bbox(item2)
        if not bbox1 or not bbox2:
            return False
        x1, y1, x2, y2 = bbox1
        X1, Y1, X2, Y2 = bbox2
        return not (x2 < X1 or x1 > X2 or y2 < Y1 or y1 > Y2)

    def spawn_enemy(self):
        # A new enemy is spawned immediately after a hit so the number of
        # active enemies stays roughly constant and the game remains lively.
        x = random.randint(50, WIDTH - 50)
        y = random.randint(-300, -50)
        enemy = self.canvas.create_rectangle(x, y, x + ENEMY_SIZE, y + ENEMY_SIZE, fill='orange')
        self.enemies.append(enemy)

    def update_score(self):
        # The score label is updated in place, so the UI never has to be rebuilt.
        self.score_label.config(text=f'Score: {self.score}')

    def end_game(self):
        # The game over state stops the animation loop and displays a message
        # telling the player how to restart.
        self.game_over = True
        self.canvas.create_text(
            WIDTH // 2,
            HEIGHT // 2,
            text='GAME OVER\nPress R to restart',
            fill='white',
            font=('Arial', 24),
            justify='center',
        )


if __name__ == '__main__':
    root = tk.Tk()
    ShootingGame(root)
    root.mainloop()
