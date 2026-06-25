# ==============================================================
# AETHEROS LINUX — DESKTOP WALLPAPER GENERATOR
# Generates a premium dark blue wallpaper with cyan holographic grid and arcs.
# ==============================================================

import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_wallpaper(output_path, width=1920, height=1080):
    print(f"Generating holographic wallpaper at {output_path}...")
    
    # 1. Base dark background (#06060a)
    img = Image.new("RGBA", (width, height), (6, 6, 10, 255))
    draw = ImageDraw.Draw(img)
    
    # 2. Draw subtle linear gradient (center glow)
    glow_layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow_layer)
    center_x, center_y = width // 2, height // 2
    
    # Draw radial glows
    for r in range(500, 0, -20):
        alpha = int(25 * (1.0 - (r / 500.0)))
        glow_draw.ellipse(
            (center_x - r, center_y - r, center_x + r, center_y + r),
            fill=(0, 242, 255, alpha)
        )
    glow_layer = glow_layer.filter(ImageFilter.GaussianBlur(30))
    img.alpha_composite(glow_layer)
    
    # 3. Draw grid lines (cyan with low opacity)
    grid_size = 80
    grid_layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    grid_draw = ImageDraw.Draw(grid_layer)
    
    for x in range(0, width, grid_size):
        # Vertical lines
        grid_draw.line([(x, 0), (x, height)], fill=(0, 242, 255, 12), width=1)
    for y in range(0, height, grid_size):
        # Horizontal lines
        grid_draw.line([(0, y), (width, y)], fill=(0, 242, 255, 12), width=1)
        
    # Draw hex-like dots at grid intersections
    for x in range(0, width, grid_size):
        for y in range(0, height, grid_size):
            grid_draw.ellipse((x - 2, y - 2, x + 2, y + 2), fill=(0, 242, 255, 30))
            
    img.alpha_composite(grid_layer)
    
    # 4. Draw futuristic circles (HUD dials) in the background
    hud_layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    hud_draw = ImageDraw.Draw(hud_layer)
    
    # Centers
    centers = [(center_x, center_y)]
    for cx, cy in centers:
        # Concentric circles
        radii = [180, 260, 340]
        for r in radii:
            hud_draw.ellipse(
                (cx - r, cy - r, cx + r, cy + r),
                outline=(0, 242, 255, 20),
                width=1
            )
            
        # Draw some angular notches
        for angle in range(0, 360, 30):
            rad = math.radians(angle)
            r1, r2 = 340, 355
            x1 = cx + r1 * math.cos(rad)
            y1 = cy + r1 * math.sin(rad)
            x2 = cx + r2 * math.cos(rad)
            y2 = cy + r2 * math.sin(rad)
            hud_draw.line([(x1, y1), (x2, y2)], fill=(0, 242, 255, 30), width=1)
            
        # Large outer brackets
        hud_draw.arc((cx - 365, cy - 365, cx + 365, cy + 365), 10, 80, fill=(0, 242, 255, 40), width=2)
        hud_draw.arc((cx - 365, cy - 365, cx + 365, cy + 365), 100, 170, fill=(0, 242, 255, 40), width=2)
        hud_draw.arc((cx - 365, cy - 365, cx + 365, cy + 365), 190, 260, fill=(0, 242, 255, 40), width=2)
        hud_draw.arc((cx - 365, cy - 365, cx + 365, cy + 365), 280, 350, fill=(0, 242, 255, 40), width=2)
        
    img.alpha_composite(hud_layer)
    
    # 5. Save final high-res wallpaper
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    img.convert("RGB").save(output_path, "PNG")
    print("Wallpaper generated successfully!")

if __name__ == "__main__":
    wp_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "themes", "aether_hologram.png")
    create_wallpaper(wp_path)
