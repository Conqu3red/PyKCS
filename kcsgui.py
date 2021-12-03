# A gui for the PyKCS command line program


from tkinter import *
from tkinter.ttk import *
from tkinter.messagebox import *
from tkinter.filedialog import *

import pykcs


class App:
    def __init__(self, window) -> None:
        self.window = window

        self.input_file = ""
        self.output_file = ""
        self.baud = IntVar(self.window, 300)
        self.leader = StringVar(self.window, "")
        self.parity = IntVar(self.window, pykcs.ParityMode.NONE)
        
        self.stopbits1 = BooleanVar(self.window, False)
        self.databits7 = BooleanVar(self.window, False)
        self.makewavefile = BooleanVar(self.window, True)

        self.baudframe()
        self.parityframe()
        self.leaderframe()
        self.bitsframe()
        self.fileframe()

    def run(self):
        if not self.input_file or not self.output_file:
            showerror("File(s) not selected", "The input/output file(s) has not been selected/does not exist.")
        
        if self.makewavefile.get():
            
            with open(self.input_file, "rb") as f:
                data = f.read()
                pykcs.encode_file(
                    data,
                    self.output_file,
                    baud=self.baud.get(),
                    data_bits=7 if self.databits7.get() else 8,
                    stop_bits=1 if self.stopbits1.get() else 2,
                    parity_mode=self.parity.get(),
                    leader=float(self.leader.get())
                )
                showinfo(f"Encoded Successfully", f"Encoded {len(data) * 8} bits.")
        else:
            parity_errors, data = pykcs.decode_file(
                self.input_file,
                baud=self.baud.get(),
                data_bits=7 if self.databits7.get() else 8,
                stop_bits=1 if self.stopbits1.get() else 2,
                parity_mode=self.parity.get(),
                leader=float(self.leader.get())
            )
            with open(self.output_file, "wb") as f:
                f.write(data)
            
            showinfo(f"Decoded Successfully", f"Decoded {len(data) * 8} bits.\n{parity_errors} parity errors.")
        

    def inputdialog(self):
        self.input_file = askopenfilename(title="Select input file")


    def outputdialog(self):
        self.output_file = asksaveasfilename(title="Select output file")

    def baudframe(self):
        bauds = (("1200", 1200),
                 ("600", 600),
                 ("300", 300))

        baudframe = Frame()
        baudframe.grid(column=0, row=1)

        t = Label(baudframe, text="Baud:")
        t.grid(padx=5, pady=5, sticky=N)

        for b in bauds:
            r = Radiobutton(
                baudframe,
                text=b[0],
                value=b[1],
                variable=self.baud,
            )
            r.grid(sticky=W)

    def leaderframe(self):
        leaderframe = Frame()
        leaderframe.grid(column=0, row=2)

        t = Label(leaderframe, text="Leader:")
        t.grid(padx=5, pady=5, sticky=W)

        e = Entry(leaderframe, textvariable=self.leader, width=6)
        e.grid(padx=5, pady=5, sticky=W)

    def parityframe(self):
        paritys = (("None", pykcs.ParityMode.NONE.value),
                   ("Odd", pykcs.ParityMode.ODD.value),
                   ("Even", pykcs.ParityMode.EVEN.value))

        parityframe = Frame()
        parityframe.grid(column=0, row=3)

        t = Label(parityframe, text="Parity:")
        t.grid(padx=5, pady=5, sticky=N)

        for b in paritys:
            r = Radiobutton(
                parityframe,
                text=b[0],
                value=b[1],
                variable=self.parity,
            )
            r.grid(sticky=W)

    def bitsframe(self):
        bitsframe = Frame()
        bitsframe.grid(column=1, row=1, sticky=N)

        t = Label(bitsframe, text="Bit Configuration:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(bitsframe, text="1 stop bit (def. 2)", variable=self.stopbits1, onvalue=True, offvalue=False)
        c1.grid(padx=5, pady=5, sticky=W)

        c2 = Checkbutton(bitsframe, text="7 data bits (def. 8)", variable=self.databits7, onvalue=True, offvalue=False)
        c2.grid(padx=5, pady=5, sticky=W)

    def fileframe(self):
        fileframe = Frame()
        fileframe.grid(column=1, row=2, sticky=N)

        t = Label(fileframe, text="File Options:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(fileframe, text="Encode File (alt: Decode File)", variable=self.makewavefile, onvalue=True,
                         offvalue=False)
        c1.grid(padx=5, pady=5, sticky=N)

        b1 = Button(fileframe, text="Input File", command=lambda: self.inputdialog())
        b1.grid(padx=5, pady=5, sticky=N)

        b2 = Button(fileframe, text="Output File", command=lambda: self.outputdialog())
        b2.grid(padx=5, pady=5, sticky=N)

        t = Label(fileframe, text="Generate File:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(fileframe, text="Encode File (alt: Decode File)", variable=self.makewavefile, onvalue=True,
                         offvalue=False)
        c1.grid(padx=5, pady=5, sticky=N)

        b2 = Button(fileframe, text="Generate Output", command=lambda: self.run())
        b2.grid(padx=5, pady=10, sticky=N)


if __name__ == "__main__":
    root = Tk()

    root.title("KCS GUI")

    app = App(root)

    root.mainloop()
