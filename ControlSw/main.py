import csv
from tkinter import *
from ctypes import windll, byref, create_unicode_buffer

# Here pyserial is utilized
import serial
import serial.tools.list_ports

# uses font Magic School One (c) 2004 Michael Hagemann

HOUSES = (("Gryffindor", '#900000'),
          ("Slytherin", '#008C45'),
          ("Ravenclaw", '#0000C9'),
          ("Hufflepuff", '#C09000'))
FILENAME = "points.csv"


def SendCmdAndGetReply(Cmd: str):
    try:
        while ser.inWaiting() > 0:
            ser.readall()
        bCmd: bytes = bytes(Cmd + '\r\n', encoding='utf-8')
        ser.write(bCmd)
        while True:
            reply: str = ser.readline().decode('utf-8').strip()
            if reply:
                return reply
            else:
                return ""
    except (OSError, serial.SerialException):
        return ""


def SendCmdAndGetOk(Cmd: str, TryCnt=1):
    while TryCnt:
        TryCnt -= 1
        answer: str = SendCmdAndGetReply(Cmd)
        # print("{0} {1}".format(Cmd, answer))
        if answer.lower().strip() == 'ok':
            return True
    return False


def find_port(PortName: str):
    # print([comport.device for comport in serial.tools.list_ports.comports()])
    global ser
    if PortName:
        try:
            ser = serial.Serial(PortName, baudrate=115200, timeout=1)
            if SendCmdAndGetOk('Ping', 2):
                print("Device found at {0}".format(PortName))
                return True
        except (OSError, serial.SerialException):
            pass
        return False
    else:
        ports = serial.tools.list_ports.comports()
        for port in ports:
            try:
                ser = serial.Serial(port.device, baudrate=115200, timeout=1)
                ser.write_timeout = 0.2
                if SendCmdAndGetOk('Ping', 3):
                    print("Device found at {0}".format(port.device))
                    return True
            except (OSError, serial.SerialException):
                pass
        return False


def loadfont(fontpath):
    FR_PRIVATE = 0x10
    pathbuf = create_unicode_buffer(fontpath)
    AddFontResourceEx = windll.gdi32.AddFontResourceExW

    flags = FR_PRIVATE
    AddFontResourceEx(byref(pathbuf), flags, 0)


def load_points():
    with open(FILENAME, "r", newline="") as file:
        reader = csv.reader(file)
        points = [int(p) for p in next(reader)]
        return points


def write_points():
    for i in range(len(points)):
        pts_lbls[i].config(text=points[i])
    # Save values
    with open(FILENAME, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(points)
    # sen values
    command = "set %d %d %d %d" % tuple(points)
    reply = SendCmdAndGetReply(command)
    print(reply)
    if not ("Ok" in reply):
        print("Error setting points")


def setpoint(i):
    entry = e_set[i]
    txt = entry.get()
    entry.delete(0, 'end')
    try:
        n = int(txt)
    except ValueError:
        return
    points[i] = n
    write_points()


def pluspoint(i):
    entry = e_plus[i]
    txt = entry.get()
    entry.delete(0, 'end')
    try:
        n = int(txt)
    except ValueError:
        return
    points[i] += n
    write_points()


def minuspoint(i):
    entry = e_minus[i]
    txt = entry.get()
    entry.delete(0, 'end')
    try:
        n = int(txt)
    except ValueError:
        return
    points[i] -= n
    write_points()


def after_enter(event):
    entry = window.focus_get()
    if entry.t == "set":
        i = entry.num
        setpoint(i)
    elif entry.t == "plus":
        i = entry.num
        pluspoint(i)
    elif entry.t == "minus":
        i = entry.num
        minuspoint(i)


fontpath = "magic-school.one.ttf"
loadfont(fontpath)

points = load_points()

window = Tk()
window.title("Hogwarts points")
pts_lbls = []
e_set = []
e_plus = []
e_minus = []

window.bind('<Return>', after_enter)

lbl = Label(text="Задать новое количество баллов (нажмите Enter для ввода)")
lbl.grid(row=2, column=0, columnspan=8)

lbl = Label(text="Добавить баллы")
lbl.grid(row=4, column=0, columnspan=8)

lbl = Label(text="Отнять баллы")
lbl.grid(row=6, column=0, columnspan=8)

btn_send = Button(
    text="Отправить", font=14, background="#272", foreground="#FFF",
    padx="11", pady="2",
    command=write_points)
btn_send.grid(row=1, column=8)

for c, house in enumerate(HOUSES):
    lbl = Label(text=house[0], font=("magic school one", 30), foreground=house[1])
    lbl.grid(row=0, column=c * 2, ipadx=10, ipady=6, padx=10, pady=10, columnspan=2)

    pts_lbl = Label(text=points[c], font=("Courier", 15))
    pts_lbl.grid(row=1, column=c * 2, ipadx=10, ipady=6, padx=10, pady=10, columnspan=2)
    pts_lbls.append(pts_lbl)

    entry_set = Entry()
    entry_set.t = "set"
    entry_set.num = c
    e_set.append(entry_set)
    entry_set.grid(row=3, column=c * 2, ipadx=20, ipady=6, pady=10, columnspan=2)

    lbl = Label(text="  +", font=("Courier", 15))
    lbl.grid(row=5, column=c * 2, ipady=6, pady=10)
    entry_plus = Entry()
    entry_plus.t = "plus"
    entry_plus.num = c
    e_plus.append(entry_plus)
    entry_plus.grid(row=5, column=c * 2 + 1, ipady=6, pady=10, padx=10)

    lbl = Label(text="  -", font=("Courier", 15))
    lbl.grid(row=7, column=c * 2, ipady=6, pady=10)
    entry_minus = Entry()
    entry_minus.t = "minus"
    entry_minus.num = c
    e_minus.append(entry_minus)
    entry_minus.grid(row=7, column=c * 2 + 1, ipady=6, pady=10, padx=10)

if __name__ == '__main__':
    PortName = sys.argv[1] if len(sys.argv) == 2 else ""
    if find_port(PortName):
        window.mainloop()
    else:
        print('Port not found')
