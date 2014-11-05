#!/usr/bin/env python3

import cairo
import sys
import parsestf

class Parser(parsestf.STFParser):
    def __init__(self, stream):
        super(Parser, self).__init__(stream)
        self.force = 0
        self.times = []

    def handle_stroke_end(self, time):
        self.ctx.stroke()
        self.force = 0

    def handle_point(self, x, y, force, time):
        ctx = self.ctx
        if force:
            if self.force:
                ctx.set_line_width(force**.3*3)
                ctx.line_to(x, y)
            else:
                ctx.move_to(x, y)
        self.force = force
        self.times.append(time)

    def parse(self, ctx, t0=None, name=None):
        self.ctx = ctx

        self.ctx.save()
        ctx.set_line_join(cairo.LINE_JOIN_ROUND)
        ctx.set_line_cap(cairo.LINE_CAP_ROUND)
        ctx.set_source_rgb(0, .06, .33)
        ctx.select_font_face("Sans", cairo.FONT_SLANT_NORMAL,
                cairo.FONT_WEIGHT_NORMAL)
        ctx.set_font_size(8)

        super(Parser, self).parse()


        title = []
        if name is not None:
            title.append(name)

        if t0 is not None:
            t = "{} to {}".format(
                time.strftime("%c", time.localtime(self.times[0]/1000.+t0)),
                time.strftime("%c", time.localtime(self.times[-1]/1000.+t0)))
            title.append(t)

        if title:
            t = " - ".join(title)
            self.ctx.save()
            self.ctx.scale(10, 10)
            #x, y, w, h, dx, dy = self.ctx.text_extents(t)
            self.ctx.move_to(30, 30)
            self.ctx.show_text(t)
            self.ctx.restore()

        self.ctx.restore()


if len(sys.argv) < 3:
    print("Usage: ./stf2pdf input.stf output.pdf [background.png]")
    sys.exit(1)

size = 4963, 6278
res = 1/10.0

surface = cairo.PDFSurface(sys.argv[2], *(i*res for i in size))

ctx = cairo.Context(surface)
ctx.scale(res, res)

if len(sys.argv) > 3:
    paper = cairo.ImageSurface.create_from_png(open(sys.argv[3], 'rb'))
    scale = size[0]/paper.get_width()
    ctx.set_source_surface(paper, 0, 0)
else:
    scale = 1
    ctx.set_source_rgb(255, 255, 255)

f = open(sys.argv[1], 'rb')
ctx.save()
ctx.scale(scale, scale)
ctx.paint()
ctx.restore()

# TODO add t0=<actual_time>
Parser(f).parse(ctx, t0=None, name="LiveScribe notes")
ctx.show_page()
