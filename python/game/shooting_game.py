import random
import tkinter as tk

WIDTH = 600
HEIGHT = 480
SHIP_SIZE = 40
BULLET_SIZE = 6
ENEMY_SIZE = 30
ENEMY_COUNT = 6

class ShootingGame(tk.Frame):
    def __init__(self, master):
        super().__init__(master)
        master.title('Flying Shooting')
        self.pack()
        self.canvas = tk.Canvas(self, width=WIDTH, height=HEIGHT, bg='navy')
        self.canvas.pack()
        self.score = 0
        self.game_over = False
        self.ship = self.canvas.create_polygon(0, 0, 0, 0, 0, 0, fill='cyan')
        self.bullets = []
        self.enemies = []
        self.create_ui()
        master.bind('<Left>', lambda e: self.move_ship(-20))
        master.bind('<Right>', lambda e: self.move_ship(20))
        master.bind('<space>', lambda e: self.fire())
        master.bind('r', lambda e: self.reset())
        self.reset()
        self.animate()

    def create_ui(self):
        self.score_label = tk.Label(self, text='Score: 0', fg='white', bg='navy', font=('Arial', 14))
        self.score_label.place(x=10, y=10)

    def reset(self):
        self.game_over = False
        self.score = 0
        self.update_score()
        self.canvas.delete('all')
        self.canvas.config(bg='navy')
        self.create_ui()
        self.bullets = []
        self.enemies = []
        self.ship_x = WIDTH // 2
        self.ship_y = HEIGHT - 60
        self.ship = self.canvas.create_polygon(0, 0, 0, 0, 0, 0, fill='cyan')
        self.draw_ship()
        for i in range(ENEMY_COUNT):
            x = random.randint(50, WIDTH - ENEMY_SIZE - 50)
            y = random.randint(-300, -50)
            enemy = self.canvas.create_rectangle(x, y, x + ENEMY_SIZE, y + ENEMY_SIZE, fill='orange')
            self.enemies.append(enemy)

    def draw_ship(self):
        coords = [self.ship_x, self.ship_y,
                  self.ship_x - SHIP_SIZE//2, self.ship_y + SHIP_SIZE,
                  self.ship_x + SHIP_SIZE//2, self.ship_y + SHIP_SIZE]
        self.canvas.coords(self.ship, *coords)

    def move_ship(self, dx):
        if self.game_over:
            return
        new_x = min(max(self.ship_x + dx, SHIP_SIZE//2), WIDTH - SHIP_SIZE//2)
        self.ship_x = new_x
        self.draw_ship()

    def fire(self):
        if self.game_over:
            return
        bullet = self.canvas.create_rectangle(self.ship_x - BULLET_SIZE//2, self.ship_y - 20,
                                              self.ship_x + BULLET_SIZE//2, self.ship_y - 10,
                                              fill='yellow')
        self.bullets.append(bullet)

    def animate(self):
        if self.game_over:
            return
        self.move_bullets()
        self.move_enemies()
        self.check_collisions()
        self.after(40, self.animate)

    def move_bullets(self):
        for bullet in self.bullets[:]:
            self.canvas.move(bullet, 0, -12)
            x1, y1, x2, y2 = self.canvas.coords(bullet)
            if y2 < 0:
                self.canvas.delete(bullet)
                self.bullets.remove(bullet)

    def move_enemies(self):
        for enemy in self.enemies:
            self.canvas.move(enemy, 0, 4)
            x1, y1, x2, y2 = self.canvas.coords(enemy)
            if y1 > HEIGHT:
                x = random.randint(50, WIDTH - ENEMY_SIZE - 50)
                y = random.randint(-300, -50)
                self.canvas.coords(enemy, x, y, x + ENEMY_SIZE, y + ENEMY_SIZE)

    def check_collisions(self):
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
        bbox1 = self.canvas.bbox(item1)
        bbox2 = self.canvas.bbox(item2)
        if not bbox1 or not bbox2:
            return False
        x1, y1, x2, y2 = bbox1
        X1, Y1, X2, Y2 = bbox2
        return not (x2 < X1 or x1 > X2 or y2 < Y1 or y1 > Y2)

    def spawn_enemy(self):
        x = random.randint(50, WIDTH - 50)
        y = random.randint(-300, -50)
        enemy = self.canvas.create_rectangle(x, y, x + ENEMY_SIZE, y + ENEMY_SIZE, fill='orange')
        self.enemies.append(enemy)

    def update_score(self):
        self.score_label.config(text=f'Score: {self.score}')

    def end_game(self):
        self.game_over = True
        self.canvas.create_text(WIDTH//2, HEIGHT//2, text='GAME OVER\nPress R to restart', fill='white', font=('Arial', 24), justify='center')

if __name__ == '__main__':
    root = tk.Tk()
    ShootingGame(root)
    root.mainloop()
