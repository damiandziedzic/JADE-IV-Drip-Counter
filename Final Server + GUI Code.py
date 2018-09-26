import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2TkAgg
from matplotlib.figure import Figure
import matplotlib.animation as animation
from matplotlib import style
import socket
import _thread as thread

import tkinter as tk

LARGE_FONT = ("Verdana", 12)
style.use("ggplot")

#Patient Class
class patient():
    def __init__(self, clientSocket, clientIPAddress, name):
        self.name = name
        self.volFlow = []
        self.vol = []
        self.timeStamp = []
        self.clientSocket = clientSocket
        self.clientIPAddress = clientIPAddress
        

#Server Function
def JADEServer(portNum, clientList, GUI):
    s = socket.socket()         # Create a socket object
    host = socket.gethostname() # Get local machine name
    port = portNum                # Reserve a port for your service.

    print ('Server started!')
    print ('Waiting for clients...')

    s.bind((host, port))        # Bind to the port
    s.listen(10)                 # Now wait for client connection.
    s.settimeout(0.1)

    while True:
        try:
           c, addr = s.accept()     # Establish connection with client.
           print("Client: ", addr)
           c.send(b'Pass?\n')
           passcode = str(c.recv(50)).strip('b').strip("'")#[:-2]
           if passcode != "abc":
               c.close()
           else:
               c.send(b'Name?\n')
               clientName = str(c.recv(50)).strip('b').strip("'")#[:-2]
               clientList.append(patient(c, addr[0], clientName))
               GUI.frames['patientList'].listPatients.insert('end', clientList[-1].name)
        except socket.timeout:
           for i in clientList:
               i.clientSocket.send(b'OK\n')
               msg = str(i.clientSocket.recv(50))
               parseMsg = msg.strip('b')
               parseMsg = parseMsg.strip("'")
               parseMsg = parseMsg.split(',')
               i.timeStamp.append(float(parseMsg[0]))
               i.vol.append(float(parseMsg[1]))
               i.volFlow.append(float(parseMsg[2]))
        else:
           for i in clientList:
               i.clientSocket.send(b'OK\n')
               msg = str(i.clientSocket.recv(50))
               parseMsg = msg.strip('b')
               parseMsg = parseMsg.strip("'")
               parseMsg = parseMsg.split(',')
               i.timeStamp.append(float(parseMsg[0]))
               i.vol.append(float(parseMsg[1]))
               i.volFlow.append(float(parseMsg[2]))
    s.close()

# live plot of graph
def animate(i, GUI, listClients, figure, volPlot, volFlowPlot):
    Selection = GUI.frames['patientList'].whichSelected()
    if Selection != "NoSelection":
        figure.suptitle(listClients[Selection].name)
        volPlot.clear()
        volPlot.plot(listClients[Selection].timeStamp, listClients[Selection].vol)
        volFlowPlot.clear()
        volFlowPlot.plot(listClients[Selection].timeStamp, listClients[Selection].volFlow)
        volFlowPlot.set_xlabel('Time (Minutes)')
        volPlot.set_ylabel('Volume (mL)')
        volFlowPlot.set_ylabel('Flow Rate (mL/min)')
    else:
        pass

#make each frame
class JADE(tk.Tk):

    def __init__(self, clientList, figure, graph1, graph2, master = None):
        
        tk.Tk.__init__(self, master)

        #change the icon from default
        tk.Tk.iconbitmap(self,default="Drop.ico")
        #change the name of the window
        tk.Tk.wm_title(self, "JADE")
        
        masterWindow = tk.Frame(self)
        masterWindow.pack(side="top", fill="both", expand = True)
        masterWindow.grid_rowconfigure(1, weight = 1)
        masterWindow.grid_columnconfigure(2, weight = 1)
        
        self.frames = {}
        self.frames['patientList'] = patientList(masterWindow, self, clientList, graph1, graph2)
        self.frames['patientList'].grid(row=0, column=0, sticky="nsew")

        GraphFrame = Graph(masterWindow, self, figure)
        self.frames['graph'] = GraphFrame
        GraphFrame.grid(row=1, column=2, sticky="nsew")

class patientList(tk.Frame):
    def __init__(self, parent, controller, clientList, subplot1, subplot2):
        tk.Frame.__init__(self,parent)
        title = tk.Label(self, text="Patient List", font=LARGE_FONT)
        title.grid(row=0, column=0, padx=100, pady=10)
        
        scrollBar = tk.Scrollbar(parent)
        scrollBar.grid(row=1, column=1, sticky="nsew")
        
        self.listPatients = tk.Listbox(parent, yscrollcommand=scrollBar.set)
        self.listPatients.grid(row=1, column=0, sticky="nsew")
        
    def whichSelected(self):
        if not self.listPatients.curselection():
            return ("NoSelection")
        else:
            return (int(self.listPatients.curselection()[0]))
        

class Graph(tk.Frame):
    def __init__(self, parent, controller, figure):
        tk.Frame.__init__(self, parent)
        canvas = FigureCanvasTkAgg(figure, self)
        canvas.draw()
        canvas._tkcanvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

def main():
    connectedClients = []
    portNumber = 8888
    f = Figure(figsize=(5,5), dpi=100)
    timeVol = f.add_subplot(211)
    timeVolFlow = f.add_subplot(212)
    timeVolFlow.set_xlabel('Time (Minutes)')
    timeVol.set_ylabel('Volume (mL)')
    timeVolFlow.set_ylabel('Flow Rate (mL/min)')
    f.suptitle('Patient Name')
    app = JADE(connectedClients, f, timeVol, timeVolFlow)
    thread.start_new_thread(JADEServer,(portNumber, connectedClients, app))
    #animate at every 1000 msec
    ani = animation.FuncAnimation(f, animate, fargs=(app, connectedClients, f, timeVol, timeVolFlow,), interval = 200)
    app.mainloop()

main()
