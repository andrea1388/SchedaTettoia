import asyncio
import serial
import threading

devouscire=False
s = serial.Serial('/dev/cu.wchusbserialfd1410', 9600)

def elabora(comando,datalen,data):
    if comando==66:
        print("presenza cancello")
    elif comando==73:
        print("temp")
    elif comando==77:
        print("stato tettoia")
        sd=data[5]+data[4]*255
        sa=5*(sd/1024)
        print("soglia: ",str(sd)," in Volt:", str(sa))
    elif comando==90:
        print("presenza tettoia")
    else:
        print("cmd non gestito:"+str(comando))

def read_serial():
    stato=0
    sum=0
    cmd=0
    len=0
    numdati=0
    dati=bytearray(10)

    while True:
        if devouscire==True: break
        reading = s.read(1)
        b=reading[0]
        #print(b)
        
        if b==65 and stato==0:
            stato=1
            

        elif stato==1:
            sum=sum+b
            cmd=b
            stato=2

        elif stato==2:
            sum=sum+b
            len=b
            if len>0:
                stato=3
                numdati=0
            else:
                stato=4

        elif stato==3:
            sum=sum+b
            dati[numdati]=b 
            numdati=numdati+1
            if numdati==len:
                stato=4


        elif stato==4:
            if sum & 255 == b:
                elabora(cmd,numdati,dati)
            stato=0
            sum=0
        

        else:
            print("errore stato")
        
        #print("stato="+str(stato))





t = threading.Thread(target=read_serial)
t.start()
while True:
    value = input("Comando:\n")
    if value=="Q":
        devouscire=True
        s.close
        print("ciao")
        break
    elif value=="T":
        buf=b'\x41\x4C\x00\x4C'
        print(buf)
        s.write(buf)

