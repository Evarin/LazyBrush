# coding: utf-8
from Tkinter import *
from PIL import Image, ImageTk, ImageDraw, ImageChops
from tkColorChooser import askcolor
from tkFileDialog import askopenfilename
from lazybrush_wrapper import lazybrush

class Window:
    def __init__(self, master):
        #Buttons
        self.bbutton = Button(root, text="Parcourir", command=self.browseimg)
        self.bbutton.pack()
        self.browseimg()

    def browseimg(self):
        Tk().withdraw() 
        filename = askopenfilename()
        if filename:
            self.bbutton.pack_forget()
            self.processimg(filename)
            self.showtools()

    def processimg(self, img):
        self.sketch = Image.open(img).convert("L")
        self.fullsketch = self.sketch.convert("RGBA")
        self.fullsketch_draw = ImageDraw.Draw(self.fullsketch)
        self.image = ImageTk.PhotoImage(self.sketch)
        wdt = self.image.width()
        hgt = self.image.height()
        root.geometry('%dx%d' % (wdt*2+200, max(hgt+50, 500)))
        self.colors = Image.new('RGBA',(wdt, hgt))
        self.colors_draw = ImageDraw.Draw(self.colors)
        self.colors_draw.rectangle([0, 0, wdt-1, hgt-1], outline = (255, 255, 255))
        self.imagesprite = Canvas(root, width=wdt, height=hgt)
        self.imagesprite.create_image((wdt/2,hgt/2), image=self.image)
        self.imagesprite.pack(side=LEFT)
        self.imagesprite.bind('<ButtonPress-1>', self.onStart)
        self.imagesprite.bind('<B1-Motion>',     self.onStart)
        self.resultsprite = Canvas(root, width=wdt, height=hgt)
        self.resultsprite.pack(side=LEFT)
        self.resultsprite.bind('<ButtonPress-1>', self.pickColor)
    
    def showtools(self):
        ## Toolbox
        # Color picker
        self.curcolor_code = "#FFFFFF"
        self.curcolor = (255, 255, 255)
        self.colorpicker = Button(root, text="Color", command=self.chooseColor, background=self.curcolor_code)
        self.colorpicker.pack(side=TOP)
        # Brush
        self.brushsize_label = Label(text="Brush size")
        self.brushsize_label.pack(side=TOP)
        self.brushsizer = Scale(root, from_=2, to=100, orient=HORIZONTAL)
        self.brushsizer.set(15)
        self.brushsizer.pack(side=TOP)
        ## Parameters
        # Lambda
        self.lambda_label = Label(text="Lambda value")
        self.lambda_label.pack(side=TOP)
        self.lambdabar = Scale(root, from_=0, to=1, resolution=0.01, orient=HORIZONTAL)
        self.lambdabar.set(0.95)
        self.lambdabar.pack(side=TOP)
        # Sigma
        self.K_label = Label(text="K scale")
        self.K_label.pack(side=TOP)
        self.Kbar = Scale(root, from_=0, to=5, resolution=0.1, orient=HORIZONTAL)
        self.Kbar.set(1)
        self.Kbar.pack(side=TOP)
        # Use LoG or pow2
        self.uselogsketch = IntVar()
        self.usetsketch = Checkbutton(root, text="Use LoG (else pow2)", variable=self.uselogsketch)
        self.usetsketch.pack(side=TOP)
        self.uselogsketch.set(1)
        # Sigma
        self.sigma_label = Label(text="Sigma value")
        self.sigma_label.pack(side=TOP)
        self.sigmabar = Scale(root, from_=0, to=4.9, resolution=0.1, orient=HORIZONTAL)
        self.sigmabar.set(1.1)
        self.sigmabar.pack(side=TOP)
        # Show LoG-filtered sketch or true sketch
        self.showlogsketch = IntVar()
        self.showtsketch = Checkbutton(root, text="Show Filtered", variable=self.showlogsketch)
        self.showtsketch.pack(side=TOP)
        # Start
        self.startlb = Button(root, text="LazyBrush", command=self.lazybrush)
        self.startlb.pack(side=TOP)
        ## Save result
        self.filename = StringVar()
        self.filefield = Entry(root, textvariable=self.filename)
        self.filename.set("results/sketch.png")
        self.filefield.pack(side=TOP)
        self.saveref = Button(root, text="Save sketch", command=self.savesketch)
        self.saveref.pack(side=TOP)
        self.saveres = Button(root, text="Save result", command=self.saveresult)
        self.saveres.pack(side=TOP)
        
    def chooseColor(self):
        (self.curcolor, self.curcolor_code) = askcolor(color=self.curcolor_code, title = "Brush color")
        self.colorpicker.config(background = self.curcolor_code)
        print self.curcolor_code
    
    def pickColor(self, event):
        if self.output_img.mode == "RGB":
            (r, g, b) = self.output_img.getpixel((event.x, event.y))
        else:
            (r, g, b, a) = self.output_img.getpixel((event.x, event.y))
        self.curcolor = (r, g, b)
        self.curcolor_code = '#%02x%02x%02x' % self.curcolor
        print(self.curcolor)
        self.colorpicker.config(background = self.curcolor_code)
        print self.curcolor
        
    def onStart(self, event):
        self.start = event
        self.drawn = None
        r = int(self.brushsizer.get()/2)
        r_c = self.brushsizer.get() - r
        self.imagesprite.create_oval(event.x-r, event.y-r, event.x+r_c, event.y+r_c, fill = self.curcolor_code, outline="")
        self.colors_draw.ellipse((event.x-r, event.y-r, event.x+r_c, event.y+r_c), fill = self.curcolor)
        self.fullsketch_draw.ellipse((event.x-r, event.y-r, event.x+r_c, event.y+r_c), fill = self.curcolor_code)

    def onGrow(self, event):
        canvas = event.widget
        if self.drawn: canvas.delete(self.drawn)
        objectId = self.shape(self.start.x, self.start.y, event.x, event.y)
        if trace: print objectId
        self.drawn = objectId
    
    def onMove(self, event):
        canvas = event.widget
        diffX, diffY = (event.x - self.start.x), (event.y - self.start.y)
        canvas.move(self.drawn, diffX, diffY)
        self.start = event
    
    def lazybrush(self):
        (img, result) = lazybrush(self.sketch, self.colors, self.sigmabar.get(), self.lambdabar.get(), self.Kbar.get(), self.uselogsketch.get())
        print result.shape
        mode = "RGBA" if self.showlogsketch.get() else "RGB"
        self.output_img = Image.fromarray(result, "RGBA").convert(mode)
        wdt = self.image.width()
        hgt = self.image.height()
        if self.showlogsketch.get():
            img = Image.fromarray(img, "L")
        else:
            img = self.sketch
        print (wdt, hgt)
        self.result = ImageChops.multiply(self.output_img, img.convert(mode))
        self.resultTk = ImageTk.PhotoImage(self.result)
        self.resultsprite.create_image((wdt/2,hgt/2), image=self.resultTk)

    def savesketch(self):
        self.fullsketch.save(self.filename.get(), 'PNG')

    def saveresult(self):
        self.result.save(self.filename.get(), 'PNG')

root = Tk()
root.geometry('200x100')
root.protocol("WM_DELETE_WINDOW", quit)
window = Window(root)
root.mainloop()
