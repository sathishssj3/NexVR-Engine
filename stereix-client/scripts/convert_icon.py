import sys
from PIL import Image

def convert_to_ico(input_path, ico_path):
    try:
        img = Image.open(input_path).convert("RGBA")
        # Ensure we use LANCZOS for high-quality downsampling
        sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]
        images = []
        for size in sizes:
            resized_img = img.resize(size, Image.Resampling.LANCZOS)
            images.append(resized_img)
            
        # The first image is the largest, pass the rest in append_images
        images[0].save(ico_path, format="ICO", sizes=sizes, append_images=images[1:])
        print("Successfully upscaled/downsampled and converted to ICO with LANCZOS")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    convert_to_ico(sys.argv[1], sys.argv[2])
