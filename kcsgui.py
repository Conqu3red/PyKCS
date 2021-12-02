# A gui for the PyKCS command line program


from tkinter import *
from tkinter.ttk import *
from tkinter.messagebox import *
from tkinter.filedialog import *
from os import system


class App:
    def __init__(self, window) -> None:
        self.window = window
        self.baudframe()
        self.parityframe()
        self.leaderframe()
        self.bitsframe()
        self.fileframe()

    def runall(self):
        python = True
        try:
            with open("version.conf", "r+") as config:
                for line in config:
                    for part in line.split():
                        if "python=" in part:
                            if "false" in part:
                                python = False
                            break
                        else:
                            break
            print("Execute with Python = " + str(python))
            config.close()
        except IOError as error:
            print(error)
            print("Warning: Reverting to defaults: Execute with Python = " + str(python))

        try:
            inputdialog
            outputdialog
        except NameError:
            showerror("File(s) not selected", "The input/output file(s) has not been selected/does not exist.")
            quit()


        command = []

        if python:
            command.append("py pykcs.py")
        elif not python:
            command.append("ckcs.exe")

        command.append(baud.get())

        if leader.get() != "":
            command.append("-L" + leader.get())

        command.append(parity.get())

        if stopbits1.get():
            command.append("-S")

        if databits7.get():
            command.append("-D")

        command.append(inputdialog)
        command.append(outputdialog)

        if makewavefile:
            command.append("-M")

        print("Command = " + " ".join(command))

        system(" ".join(command))

        print("Done.")

    def inputdialog(self):
        global inputdialog
        inputdialog = askopenfilename(title="Select input file")


    def outputdialog(self):
        global outputdialog
        outputdialog = asksaveasfilename(title="Select output file")

    def baudframe(self):
        bauds = (("1200", "-B2"),
                 ("600", "-B1"),
                 ("300", ""))

        global baud

        baud = StringVar(self.window, "")

        baudframe = Frame()
        baudframe.grid(column=0, row=1)

        t = Label(baudframe, text="Baud:")
        t.grid(padx=5, pady=5, sticky=N)

        for b in bauds:
            r = Radiobutton(
                baudframe,
                text=b[0],
                value=b[1],
                variable=baud,
            )
            r.grid(padx=5, pady=5, sticky=N)

    def leaderframe(self):
        global leader

        leader = StringVar(self.window, "")

        leaderframe = Frame()
        leaderframe.grid(column=0, row=2)

        t = Label(leaderframe, text="Leader:")
        t.grid(padx=5, pady=5, sticky=N)

        e = Entry(leaderframe, textvariable=leader, width=6)
        e.grid(padx=5, pady=5, sticky=N)

    def parityframe(self):
        paritys = (("None", ""),
                   ("Odd", "-O"),
                   ("Even", "-E"))

        global parity

        parity = StringVar(self.window, "")

        parityframe = Frame()
        parityframe.grid(column=0, row=3)

        t = Label(parityframe, text="Parity:")
        t.grid(padx=5, pady=5, sticky=N)

        for b in paritys:
            r = Radiobutton(
                parityframe,
                text=b[0],
                value=b[1],
                variable=parity,
            )
            r.grid(padx=5, pady=5, sticky=N)

    def bitsframe(self):
        global stopbits1
        global databits7

        stopbits1 = BooleanVar(self.window, False)
        databits7 = BooleanVar(self.window, False)

        bitsframe = Frame()
        bitsframe.grid(column=1, row=1, sticky=N)

        t = Label(bitsframe, text="Bit Configuration:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(bitsframe, text="1 stop bit (def. 2)", variable=stopbits1, onvalue=True, offvalue=False)
        c1.grid(padx=5, pady=5, sticky=N)

        c2 = Checkbutton(bitsframe, text="7 data bits (def. 8)", variable=databits7, onvalue=True, offvalue=False)
        c2.grid(padx=5, pady=5, sticky=N)

    def fileframe(self):
        global makewavefile

        makewavefile = BooleanVar(self.window, True)

        fileframe = Frame()
        fileframe.grid(column=1, row=2, sticky=N)

        t = Label(fileframe, text="File Options:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(fileframe, text="Encode File (alt: Decode File)", variable=makewavefile, onvalue=True,
                         offvalue=False)
        c1.grid(padx=5, pady=5, sticky=N)

        b1 = Button(fileframe, text="Input File", command=lambda: self.inputdialog())
        b1.grid(padx=5, pady=5, sticky=N)

        b2 = Button(fileframe, text="Output File", command=lambda: self.outputdialog())
        b2.grid(padx=5, pady=5, sticky=N)

        t = Label(fileframe, text="Generate File:")
        t.grid(padx=5, pady=5, sticky=N)

        c1 = Checkbutton(fileframe, text="Encode File (alt: Decode File)", variable=makewavefile, onvalue=True,
                         offvalue=False)
        c1.grid(padx=5, pady=5, sticky=N)

        b2 = Button(fileframe, text="Generate Output", command=lambda: self.runall())
        b2.grid(padx=5, pady=10, sticky=N)


if __name__ == "__main__":
    root = Tk()

    root.title("KCS GUI")

    app = App(root)

    root.mainloop()
