"""Desktop preview of the current 220x176 TriangleFace firmware face.

Renders with numpy/Pillow rather than Tkinter's own vector shapes, since
this needs to reproduce the firmware's actual per-pixel vertical shading
gradient (see verticalShadeScale() in
firmware/src/faces/triangle_face.cpp) - a plain flat-color polygon fill,
which is all Tkinter's canvas can do on its own, can't show that.
"""

import math
import time
import tkinter as tk

import numpy as np
from PIL import Image, ImageDraw, ImageTk

WIDTH = 220
HEIGHT = 176
SCALE = 3
PI = math.pi
SHADE_MIN_SCALE = 0.45  # see verticalShadeScale()'s comment in the firmware


def scale_color(rgb, scale):
    scale = max(0.0, min(1.0, scale))
    r, g, b = rgb
    return (int(r * scale + 0.5), int(g * scale + 0.5), int(b * scale + 0.5))


def vertical_shade_scale(y, top_y, bottom_y):
    span = max(1.0, bottom_y - top_y)
    t = (y - top_y) / span
    t = max(0.0, min(1.0, t))
    return SHADE_MIN_SCALE + (1.0 - SHADE_MIN_SCALE) * t


def fill_triangle_graded(img, x0, y0, x1, y1, x2, y2, color, shade_top, shade_bottom):
    """Same barycentric fill + per-row shading as
    fillTriangleGradedInBuffer() in the firmware, vectorized across each
    row's columns (rather than a fully per-pixel Python loop) since this
    runs every frame at interactive speed."""
    min_x = max(0, int(math.floor(min(x0, x1, x2))))
    max_x = min(WIDTH - 1, int(math.ceil(max(x0, x1, x2))))
    min_y = max(0, int(math.floor(min(y0, y1, y2))))
    max_y = min(HEIGHT - 1, int(math.ceil(max(y0, y1, y2))))
    if min_x > max_x or min_y > max_y:
        return

    xs = np.arange(min_x, max_x + 1)
    for y in range(min_y, max_y + 1):
        w0 = (x1 - x0) * (y - y0) - (y1 - y0) * (xs - x0)
        w1 = (x2 - x1) * (y - y1) - (y2 - y1) * (xs - x1)
        w2 = (x0 - x2) * (y - y2) - (y0 - y2) * (xs - x2)
        mask = ((w0 >= 0) & (w1 >= 0) & (w2 >= 0)) | ((w0 <= 0) & (w1 <= 0) & (w2 <= 0))
        if not mask.any():
            continue
        shaded = scale_color(color, vertical_shade_scale(y, shade_top, shade_bottom))
        img[y, min_x:max_x + 1][mask] = shaded


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
        self.label = tk.Label(root, background="#000000", borderwidth=0)
        self.label.pack()
        self.started = time.monotonic()
        self.frame = 0
        self.photo = None  # kept alive to avoid Tkinter garbage-collecting it
        root.bind("<Escape>", lambda _event: root.destroy())
        self.tick()

    def draw(self):
        phase = self.frame * 0.09
        illumination = (
            0.82
            + 0.10 * math.sin(phase)
            + 0.05 * math.sin(phase * 2.37)
            + 0.03 * math.sin(phase * 5.11)
        )
        face_color = (int(255 * illumination), int(145 * illumination), 0)

        img = np.zeros((HEIGHT, WIDTH, 3), dtype=np.uint8)

        center_x = WIDTH / 2
        eye_y = HEIGHT * 0.3
        eye_size = WIDTH * 0.12
        eye_offset = WIDTH * 0.28

        nose_y = HEIGHT * 0.46
        nose_size = eye_size * 0.55

        mouth_y = HEIGHT * 0.60
        mouth_left = WIDTH * 0.18
        mouth_right = WIDTH * 0.82
        upper_depth = HEIGHT * 0.10
        lower_depth = HEIGHT * 0.24

        # One shared light source for every carved opening, same as the
        # firmware - see verticalShadeScale()'s comment there.
        shade_top = eye_y - eye_size
        shade_bottom = mouth_y + lower_depth

        fill_triangle_graded(
            img, center_x - eye_offset, eye_y - eye_size,
            center_x - eye_offset - eye_size, eye_y + eye_size,
            center_x - eye_offset + eye_size, eye_y + eye_size,
            face_color, shade_top, shade_bottom)
        fill_triangle_graded(
            img, center_x + eye_offset, eye_y - eye_size,
            center_x + eye_offset - eye_size, eye_y + eye_size,
            center_x + eye_offset + eye_size, eye_y + eye_size,
            face_color, shade_top, shade_bottom)
        fill_triangle_graded(
            img, center_x, nose_y - nose_size,
            center_x - nose_size, nose_y + nose_size,
            center_x + nose_size, nose_y + nose_size,
            face_color, shade_top, shade_bottom)

        for x in range(math.floor(mouth_left), math.ceil(mouth_right) + 1):
            upper_y = smile_arc_y(mouth_y, upper_depth, x, mouth_left, mouth_right)
            lower_y = smile_arc_y(mouth_y, lower_depth, x, mouth_left, mouth_right)
            y0 = max(0, math.floor(upper_y))
            y1 = min(HEIGHT - 1, math.ceil(lower_y))
            if y0 > y1 or not (0 <= x < WIDTH):
                continue
            ys = np.arange(y0, y1 + 1)
            scales = SHADE_MIN_SCALE + (1.0 - SHADE_MIN_SCALE) * np.clip(
                (ys - shade_top) / max(1.0, shade_bottom - shade_top), 0.0, 1.0)
            col = np.stack([
                np.clip(face_color[0] * scales + 0.5, 0, 255),
                np.clip(face_color[1] * scales + 0.5, 0, 255),
                np.clip(face_color[2] * scales + 0.5, 0, 255),
            ], axis=1).astype(np.uint8)
            img[y0:y1 + 1, x] = col

        pil_img = Image.fromarray(img, "RGB")
        draw = ImageDraw.Draw(pil_img)

        tooth_width = WIDTH * 0.045
        tooth_height = HEIGHT * 0.07
        tooth_offset = WIDTH * 0.01
        tooth_spacing = WIDTH * 0.15
        for tooth in range(3):
            tooth_center = center_x + (tooth - 1) * tooth_spacing
            tooth_y = smile_arc_y(mouth_y, upper_depth, tooth_center,
                                  mouth_left, mouth_right)
            draw.polygon(
                tangent_tooth(tooth_center, tooth_y, tooth_width, tooth_height,
                              tooth_offset, upper_depth, mouth_left, mouth_right,
                              True),
                fill=(0, 0, 0))

        for tooth in range(2):
            tooth_center = center_x + (tooth * 2 - 1) * tooth_spacing / 2
            tooth_y = smile_arc_y(mouth_y, lower_depth, tooth_center,
                                  mouth_left, mouth_right)
            draw.polygon(
                tangent_tooth(tooth_center, tooth_y, tooth_width, tooth_height,
                              tooth_offset, lower_depth, mouth_left, mouth_right,
                              False),
                fill=(0, 0, 0))

        pil_img = pil_img.resize((WIDTH * SCALE, HEIGHT * SCALE), Image.NEAREST)
        self.photo = ImageTk.PhotoImage(pil_img)
        self.label.configure(image=self.photo)

    def tick(self):
        self.draw()
        self.frame += 1
        self.root.after(16, self.tick)


if __name__ == "__main__":
    window = tk.Tk()
    FacePreview(window)
    window.mainloop()
