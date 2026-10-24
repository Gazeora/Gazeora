"""Draws the MSI wizard's pictures in Gazeora's colors (apps/gazeora/src/ui/theme.h).

Run after changing the logo or the colors, and commit the .bmp files:
    python installer/branding/make_branding.py
Needs Pillow. Windows Installer draws its black text over these, so the text areas stay white.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = Path(__file__).resolve().parent
LOGO = HERE.parents[1] / "apps" / "gazeora" / "resources" / "gazeora-256.png"
FONT = Path("C:/Windows/Fonts/seguisb.ttf")  # Segoe UI Semibold, the launcher's font


class Palette:
    """Colors from the launcher's theme."""

    SIDEBAR = (13, 14, 22)
    WINDOW = (18, 20, 30)
    ACCENT = (124, 92, 255)
    ACCENT_END = (56, 189, 248)
    TEXT = (236, 238, 246)
    TEXT_DIM = (152, 158, 182)
    PAPER = (255, 255, 255)


class Painter:
    """Drawing helpers shared by both pictures."""

    def __init__(self, logo_path: Path, font_path: Path):
        self._logo = Image.open(logo_path).convert("RGBA")
        self._font_path = font_path

    def logo(self, size: int) -> Image.Image:
        return self._logo.resize((size, size), Image.LANCZOS)

    def font(self, size: int) -> ImageFont.FreeTypeFont:
        return ImageFont.truetype(str(self._font_path), size)

    @staticmethod
    def navy(size: tuple) -> Image.Image:
        """The sidebar's navy, fading into the window's, with the accent glowing in it."""
        width, height = size
        panel = Image.new("RGB", size)
        draw = ImageDraw.Draw(panel)
        for y in range(height):
            draw.line([(0, y), (width, y)], fill=Painter.mix(Palette.SIDEBAR, Palette.WINDOW, y / height))
        glow = Image.new("RGB", size, (0, 0, 0))
        glow_draw = ImageDraw.Draw(glow)
        glow_draw.ellipse([-width * 0.4, -height * 0.15, width * 0.9, height * 0.45], fill=Palette.ACCENT)
        glow_draw.ellipse([width * 0.2, height * 0.7, width * 1.4, height * 1.2], fill=Palette.ACCENT_END)
        glow = glow.filter(ImageFilter.GaussianBlur(max(width, height) / 6))
        return Image.blend(panel, Image.composite(glow, panel, Image.new("L", size, 255)), 0.22)

    @staticmethod
    def accent_line(size: tuple, vertical: bool) -> Image.Image:
        """The violet to cyan gradient of the launcher's primary buttons."""
        width, height = size
        line = Image.new("RGB", size)
        draw = ImageDraw.Draw(line)
        steps = height if vertical else width
        for i in range(steps):
            color = Painter.mix(Palette.ACCENT, Palette.ACCENT_END, i / max(steps - 1, 1))
            if vertical:
                draw.line([(0, i), (width, i)], fill=color)
            else:
                draw.line([(i, 0), (i, height)], fill=color)
        return line

    @staticmethod
    def mix(a: tuple, b: tuple, t: float) -> tuple:
        return tuple(round(x + (y - x) * t) for x, y in zip(a, b))


class DialogPicture:
    """Welcome and finish pages: the brand on the left, white where the text goes."""

    SIZE = (493, 312)
    PANEL_WIDTH = 164
    LOGO_SIZE = 104

    def __init__(self, painter: Painter):
        self._painter = painter

    def render(self) -> Image.Image:
        picture = Image.new("RGB", self.SIZE, Palette.PAPER)
        panel_size = (self.PANEL_WIDTH, self.SIZE[1])
        picture.paste(self._painter.navy(panel_size), (0, 0))
        picture.paste(self._painter.accent_line((3, self.SIZE[1]), vertical=True), (self.PANEL_WIDTH - 3, 0))
        logo = self._painter.logo(self.LOGO_SIZE)
        logo_left = (self.PANEL_WIDTH - self.LOGO_SIZE) // 2
        picture.paste(logo, (logo_left, 78), logo)
        self._centered_text(picture, "Gazeora", 196, 24, Palette.TEXT)
        self._centered_text(picture, "VR game launcher", 230, 11, Palette.TEXT_DIM)
        return picture

    def _centered_text(self, picture: Image.Image, text: str, top: int, size: int, color: tuple):
        draw = ImageDraw.Draw(picture)
        font = self._painter.font(size)
        width = draw.textlength(text, font=font)
        draw.text(((self.PANEL_WIDTH - 3 - width) / 2, top), text, font=font, fill=color)


class BannerPicture:
    """The strip above every other page: white under the title, the logo on navy at the right."""

    SIZE = (493, 58)
    BLOCK_LEFT = 420  # the page description reaches about 406 px
    LOGO_SIZE = 40
    LINE_HEIGHT = 2

    def __init__(self, painter: Painter):
        self._painter = painter

    def render(self) -> Image.Image:
        width, height = self.SIZE
        picture = Image.new("RGB", self.SIZE, Palette.PAPER)
        block_size = (width - self.BLOCK_LEFT, height - self.LINE_HEIGHT)
        picture.paste(self._painter.navy(block_size), (self.BLOCK_LEFT, 0))
        logo = self._painter.logo(self.LOGO_SIZE)
        logo_top = (height - self.LINE_HEIGHT - self.LOGO_SIZE) // 2
        logo_left = self.BLOCK_LEFT + (block_size[0] - self.LOGO_SIZE) // 2
        picture.paste(logo, (logo_left, logo_top), logo)
        line = self._painter.accent_line((width, self.LINE_HEIGHT), vertical=False)
        picture.paste(line, (0, height - self.LINE_HEIGHT))
        return picture


def main():
    painter = Painter(LOGO, FONT)
    DialogPicture(painter).render().save(HERE / "dialog.bmp")
    BannerPicture(painter).render().save(HERE / "banner.bmp")


if __name__ == "__main__":
    main()
