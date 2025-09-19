import os
try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Pillow not found. Trying to install...")
    os.system("pip install pillow")
    from PIL import Image, ImageDraw, ImageFont

def create_icon():
    # Create a 256x256 image with a transparent background
    img = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    
    # Get a drawing context
    draw = ImageDraw.Draw(img)
    
    # Draw a blue circle as background
    draw.ellipse((20, 20, 236, 236), fill=(0, 100, 200, 255))
    
    # Draw a simple 'T' in the center
    try:
        # Try to load a font
        font = ImageFont.truetype("arial.ttf", 150)
    except IOError:
        # Fall back to default font
        try:
            font = ImageFont.load_default()
        except:
            # If all else fails, use bitmap font
            font = None
    
    # Draw the text
    if font:
        # Calculate text size to center it
        text = "T"
        try:
            text_width, text_height = draw.textsize(text, font=font)
        except:
            # For newer Pillow versions
            try:
                text_width, text_height = draw.textbbox((0, 0), text, font=font)[2:]
            except:
                text_width, text_height = 100, 100
                
        position = ((256 - text_width) // 2, (256 - text_height) // 2 - 10)
        draw.text(position, text, fill=(255, 255, 255, 255), font=font)
    else:
        # Simple fallback if no font is available
        draw.rectangle((90, 60, 166, 80), fill=(255, 255, 255, 255))
        draw.rectangle((118, 80, 138, 196), fill=(255, 255, 255, 255))
    
    # Save as .ico file
    img.save("Tocin_Logo.ico", format="ICO")
    print(f"Icon created at {os.path.abspath('Tocin_Logo.ico')}")

if __name__ == "__main__":
    create_icon() 
