"""Desktop preview of the current 220x176 TriangleFace firmware face."""

import math
import time
import tkinter as tk

WIDTH = 220
HEIGHT = 176
SCALE = 3
PI = math.pi


def rgb565(red, green, blue):
    red = (int(red) >> 3) << 3
    green = (int(green) >> 2) << 2
    blue = (int(blue) >> 3) << 3
    return f"#{red:02x}{green:02x}{blue:02x}"


def smile_arc_y(corner_y, depth, x, left_x, right_x):
    progress = (x - left_x) / (right_x - left_x)
    return corner_y + depth * math.sin(PI * progress)


def tangent_tooth(center_x, base_y, width, height, base_offset, depth,
                  left_x, right_x, hangs_down):
    progress = (center_x - left_x) / (right_x - left_x)
    slope = depth * PI * math.cos(PI * progress) / (right_x - left_x)
    tangent_length = math.sqrt(1.0 + slope * slope)
    tangent_x = 1.0 / tangent_length
    tangent_y = slope / tangent_length
    normal_x = -tangent_y if hangs_down else tangent_y
    normal_y = tangent_x if hangs_down else -tangent_x
    base_center_x = center_x - normal_x * base_offset
    base_center_y = base_y - normal_y * base_offset

    point1 = (base_center_x - tangent_x * width / 2,
              base_center_y - tangent_y * width / 2)
    point2 = (base_center_x + tangent_x * width / 2,
              base_center_y + tangent_y * width / 2)
    point3 = (point2[0] + normal_x * height,
              point2[1] + normal_y * height)
    point4 = (point1[0] + normal_x * height,
              point1[1] + normal_y * height)
    return point1, point2, point3, point4


class FacePreview:
    def __init__(self, root):
        self.root = root
        self.root.title("Jabberin' Jack - TriangleFace preview")
        self.canvas = tk.Canvas(
            root,
            width=WIDTH * SCALE,
            height=HEIGHT * SCALE,
            background="#000000",
            highlightthickness=0,
        )
        self.canvas.pack()
        self.started = time.monotonic()
        self.frame = 0
        root.bind("<Escape>", lambda _event: root.destroy())
        self.tick()

    def point_scale(self, point):
        return tuple(round(value * SCALE) for value in point)

    def polygon(self, points, color):
        scaled = [coordinate for point in points for coordinate in self.point_scale(point)]
        self.canvas.create_polygon(*scaled, fill=color, outline=color)

    def draw(self):
        self.canvas.delete("all")
        phase = self.frame * 0.09
        illumination = (
            0.82
            + 0.10 * math.sin(phase)
            + 0.05 * math.sin(phase * 2.37)
            + 0.03 * math.sin(phase * 5.11)
        )
        face_color = rgb565(255 * illumination, 145 * illumination, 0)
        center_x = WIDTH / 2
        eye_y = HEIGHT * 0.3
        eye_size = WIDTH * 0.12
        eye_offset = WIDTH * 0.28

        self.polygon(
            [(center_x - eye_offset, eye_y - eye_size),
             (center_x - eye_offset - eye_size, eye_y + eye_size),
             (center_x - eye_offset + eye_size, eye_y + eye_size)],
            face_color,
        )
        self.polygon(
            [(center_x + eye_offset, eye_y - eye_size),
             (center_x + eye_offset - eye_size, eye_y + eye_size),
             (center_x + eye_offset + eye_size, eye_y + eye_size)],
            face_color,
        )

        nose_y = HEIGHT * 0.46
        nose_size = eye_size * 0.55
        self.polygon(
            [(center_x, nose_y - nose_size),
             (center_x - nose_size, nose_y + nose_size),
             (center_x + nose_size, nose_y + nose_size)],
            face_color,
        )

        mouth_y = HEIGHT * 0.60
        mouth_left = WIDTH * 0.18
        mouth_right = WIDTH * 0.82
        upper_depth = HEIGHT * 0.10
        lower_depth = HEIGHT * 0.24
        for x in range(math.floor(mouth_left), math.ceil(mouth_right) + 1):
            upper_y = smile_arc_y(mouth_y, upper_depth, x, mouth_left, mouth_right)
            lower_y = smile_arc_y(mouth_y, lower_depth, x, mouth_left, mouth_right)
            self.canvas.create_rectangle(
                x * SCALE,
                upper_y * SCALE,
                (x + 1) * SCALE,
                (lower_y + 1) * SCALE,
                fill=face_color,
                outline=face_color,
            )

        tooth_width = WIDTH * 0.045
        tooth_height = HEIGHT * 0.07
        tooth_offset = WIDTH * 0.01
        tooth_spacing = WIDTH * 0.15
        for tooth in range(3):
            tooth_center = center_x + (tooth - 1) * tooth_spacing
            tooth_y = smile_arc_y(mouth_y, upper_depth, tooth_center,
                                  mouth_left, mouth_right)
            self.polygon(
                tangent_tooth(tooth_center, tooth_y, tooth_width, tooth_height,
                              tooth_offset, upper_depth, mouth_left, mouth_right,
                              True),
                "#000000",
            )

        for tooth in range(2):
            tooth_center = center_x + (tooth * 2 - 1) * tooth_spacing / 2
            tooth_y = smile_arc_y(mouth_y, lower_depth, tooth_center,
                                  mouth_left, mouth_right)
            self.polygon(
                tangent_tooth(tooth_center, tooth_y, tooth_width, tooth_height,
                              tooth_offset, lower_depth, mouth_left, mouth_right,
                              False),
                "#000000",
            )

    def tick(self):
        self.draw()
        self.frame += 1
        self.root.after(16, self.tick)


if __name__ == "__main__":
    window = tk.Tk()
    FacePreview(window)
    window.mainloop()
