import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_outer_ring(path):
    size = 400
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    
    # Outer circle border
    draw.ellipse([cx - 190, cy - 190, cx + 190, cy + 190], outline=(6, 182, 212, 100), width=1)
    
    # Dashed ring
    num_dashes = 36
    dash_extent = 6
    for i in range(num_dashes):
        start_angle = i * (360 / num_dashes)
        draw.arc([cx - 170, cy - 170, cx + 170, cy + 170], 
                 start_angle, start_angle + dash_extent, 
                 fill=(6, 182, 212, 255), width=3)
                 
    # Inner thin ring
    draw.ellipse([cx - 150, cy - 150, cx + 150, cy + 150], outline=(6, 182, 212, 120), width=1)
    
    # Ticks on the inner ring
    for angle_deg in range(0, 360, 15):
        rad = math.radians(angle_deg)
        x1 = cx + 150 * math.cos(rad)
        y1 = cy + 150 * math.sin(rad)
        x2 = cx + 156 * math.cos(rad)
        y2 = cy + 156 * math.sin(rad)
        draw.line([x1, y1, x2, y2], fill=(6, 182, 212, 200), width=2)
        
    img.save(os.path.join(path, "ring_outer.png"))

def create_middle_ring(path):
    size = 300
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    
    # Hexagon pattern or nodes
    draw.ellipse([cx - 120, cy - 120, cx + 120, cy + 120], outline=(2, 132, 199, 80), width=2)
    
    # Draw triangular teeth on the ring
    num_teeth = 12
    for i in range(num_teeth):
        angle_deg = i * (360 / num_teeth)
        rad = math.radians(angle_deg)
        
        # Tip of tooth
        tx = cx + 120 * math.cos(rad)
        ty = cy + 120 * math.sin(rad)
        
        # Base of tooth
        rad_left = math.radians(angle_deg - 4)
        rad_right = math.radians(angle_deg + 4)
        bx1 = cx + 110 * math.cos(rad_left)
        by1 = cy + 110 * math.sin(rad_left)
        bx2 = cx + 110 * math.cos(rad_right)
        by2 = cy + 110 * math.sin(rad_right)
        
        draw.polygon([tx, ty, bx1, by1, bx2, by2], fill=(2, 132, 199, 200), outline=(2, 132, 199, 255))
        
    img.save(os.path.join(path, "ring_mid.png"))

def create_inner_ring(path):
    size = 200
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    
    # 8 spoke lines
    num_spokes = 8
    for i in range(num_spokes):
        angle_deg = i * (360 / num_spokes)
        rad = math.radians(angle_deg)
        x1 = cx + 25 * math.cos(rad)
        y1 = cy + 25 * math.sin(rad)
        x2 = cx + 75 * math.cos(rad)
        y2 = cy + 75 * math.sin(rad)
        draw.line([x1, y1, x2, y2], fill=(6, 182, 212, 255), width=3)
        
    # Core inner ring
    draw.ellipse([cx - 75, cy - 75, cx + 75, cy + 75], outline=(6, 182, 212, 255), width=2)
    draw.ellipse([cx - 25, cy - 25, cx + 25, cy + 25], outline=(6, 182, 212, 255), width=2)
    
    img.save(os.path.join(path, "ring_inner.png"))

def create_core_glow(path):
    size = 100
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = size // 2, size // 2
    
    # Draw nested circles with alpha gradient
    for r in range(45, 0, -2):
        alpha = int(255 * (1 - r / 45) ** 2)
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(6, 182, 212, alpha), outline=None)
        
    img.save(os.path.join(path, "core_glow.png"))

def create_hud_brackets(path):
    w, h = 1920, 1080
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Accent color
    cyan = (6, 182, 212, 180)
    dark_cyan = (6, 182, 212, 40)
    grid_color = (30, 41, 59, 80)
    
    # 1. Corner L-brackets
    pad = 60
    length = 80
    thick = 3
    
    # Top-Left
    draw.line([pad, pad, pad + length, pad], fill=cyan, width=thick)
    draw.line([pad, pad, pad, pad + length], fill=cyan, width=thick)
    # Top-Right
    draw.line([w - pad, pad, w - pad - length, pad], fill=cyan, width=thick)
    draw.line([w - pad, pad, w - pad, pad + length], fill=cyan, width=thick)
    # Bottom-Left
    draw.line([pad, h - pad, pad + length, h - pad], fill=cyan, width=thick)
    draw.line([pad, h - pad, pad, h - pad - length], fill=cyan, width=thick)
    # Bottom-Right
    draw.line([w - pad, h - pad, w - pad - length, h - pad], fill=cyan, width=thick)
    draw.line([w - pad, h - pad, w - pad, h - pad - length], fill=cyan, width=thick)
    
    # 2. Side visualizer markings
    # Left edge ticks
    for y in range(h // 2 - 200, h // 2 + 200, 20):
        draw.line([pad + 10, y, pad + 25, y], fill=cyan, width=1)
    # Center targeting gridlines
    draw.line([pad, h // 2, pad + 40, h // 2], fill=cyan, width=2)
    draw.line([w - pad, h // 2, w - pad - 40, h // 2], fill=cyan, width=2)
    
    # 3. Outer border bounds
    draw.rectangle([pad + 5, pad + 5, w - pad - 5, h - pad - 5], outline=dark_cyan, width=1)
    
    # 4. Draw horizontal scanning gridlines
    for y in range(pad + 100, h - pad - 100, 80):
        draw.line([pad + 40, y, w - pad - 40, y], fill=grid_color, width=1)
        
    img.save(os.path.join(path, "hud_brackets.png"))

def create_progress_images(path):
    # Progress Track Background
    bg_w, bg_h = 500, 16
    bg_img = Image.new("RGBA", (bg_w, bg_h), (0, 0, 0, 0))
    bg_draw = ImageDraw.Draw(bg_img)
    bg_draw.rounded_rectangle([0, 0, bg_w, bg_h], radius=8, fill=(15, 23, 42, 220), outline=(30, 41, 59, 255), width=2)
    bg_img.save(os.path.join(path, "progress_bg.png"))
    
    # Progress Fill base segment
    fill_w, fill_h = 500, 16
    fill_img = Image.new("RGBA", (fill_w, fill_h), (0, 0, 0, 0))
    fill_draw = ImageDraw.Draw(fill_img)
    fill_draw.rounded_rectangle([2, 2, fill_w - 2, fill_h - 2], radius=6, fill=(6, 182, 212, 255), outline=None)
    fill_img.save(os.path.join(path, "progress_fill.png"))

if __name__ == "__main__":
    target_dir = os.path.dirname(os.path.abspath(__file__))
    print(f"Generating assets inside {target_dir}...")
    create_outer_ring(target_dir)
    create_middle_ring(target_dir)
    create_inner_ring(target_dir)
    create_core_glow(target_dir)
    create_hud_brackets(target_dir)
    create_progress_images(target_dir)
    print("Asset generation complete!")
