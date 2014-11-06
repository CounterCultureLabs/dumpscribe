#!/usr/bin/env python3

import time
import cairo
import sys
import parsestf

class STF2PDF(parsestf.STFParser):
    def __init__(self, stream):
        super(STF2PDF, self).__init__(stream)
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

        super(STF2PDF, self).parse()


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

    # outfile is path to resulting pdf file
    # background is optional png file to use as background for the pdf
    def convert(self, outfile, background=None):

        if background:
            try:
                f_bg = open(background, 'rb')
            except:
                f_bg = None

        size = 4963, 6278
        res = 1/10.0

        surface = cairo.PDFSurface(outfile, *(i*res for i in size))

        ctx = cairo.Context(surface)
        ctx.scale(res, res)
        ctx.save()

        if f_bg:
            paper = cairo.ImageSurface.create_from_png(f_bg)
            scale = size[0]/paper.get_width()
            ctx.scale(scale, scale)
            ctx.set_source_surface(paper, 0, 0)
            f_bg.close()
        else:
            scale = 1
            ctx.scale(scale, scale)
            ctx.set_source_rgb(255, 255, 255)


        ctx.paint()
        ctx.restore()

        # TODO figure out time
        # We need to have the time from pen info in order to parse times? Maybe?
        # info = pen.get_info()
        # t0 = ET.fromstring(info).find("peninfo/time")
        # t0 = time.time() - float(t0.get("absolute"))/1000
        self.parse(ctx, t0=None, name="LiveScribe notes")
        ctx.show_page()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: ./stf2pdf input.stf output.pdf [background.png]")
        sys.exit(1)
        
    f = open(sys.argv[1], 'rb')

    if len(sys.argv) > 3:
        STF2PDF(f).convert(sys.argv[2], sys.argv[3])
    else:
        STF2PDF(f).convert(sys.argv[2])
