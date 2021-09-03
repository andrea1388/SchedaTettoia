#!/usr/bin/env python3
import asyncio
import serial
import threading
from paho.mqtt.client import Client
client = Client(client_id = "Client485")


devouscire=False
s = serial.Serial('/dev/serial/by-path/pci-0000:00:1d.1-usb-0:2:1.0-port0', 9600)
# s = serial.Serial('/dev/cu.wchusbserialfa1340', 9600)


def elabora(comando,datalen,data):
    if comando==66:
        print("presenza cancello")
    elif comando==67: # C
        print("pulsante cancello esterno")
        client.publish(topic = "cancello/apriporta", payload = "ON")
    elif comando==68: # D
        print("modo notte")
        client.publish(topic = "modo/notte", payload = "ON")
    elif comando==69: # E
        print("modo giorno")
        client.publish(topic = "modo/notte", payload = "OFF")
    elif comando==73:
        # print("temp")
    elif comando==74: # J
        print("allarme")
        client.publish(topic = "home/alarm", payload = "triggered")
    elif comando==77:
        print("stato tettoia")
        sd=data[5]+data[4]*255
        sa=5*(sd/1024)
        print("soglia: ",str(sd)," in Volt:", str(sa))
        sd=data[10]+data[9]*255
        sa=5*(sd/1024)
        print("valore: ",str(sd)," in Volt:", str(sa))
    elif comando==82: # R
        print("armato fuori casa")
        client.publish(topic = "home/alarm", payload = "armed_away")
    elif comando==83: # S
        print("disarmato")
        client.publish(topic = "home/alarm", payload = "disarmed")
    elif comando==84: # T
        print("tag cancello esterno")
        client.publish(topic = "cancello/tag", payload = "ON")
    elif comando==85: # U
        print("armato in casa")
        client.publish(topic = "home/alarm", payload = "armed_home")
    elif comando==90: # Z
        print("presenza tettoia")
        client.publish(topic = "portaesterna/presenza", payload = "ON")
    elif comando==ord('e'): # fari accesi
        print("fari accesi")
        client.publish(topic = "tettoia/fari", payload = "ON")
    elif comando==ord('f'): # fari spenti
        print("fari spenti")
        client.publish(topic = "tettoia/fari", payload = "OFF")
    elif comando==ord('g'): # lampada accesa
        print("lampada accesa")
        client.publish(topic = "tettoia/lampada", payload = "accendilampadatettoia")
    elif comando==ord('h'): # lampada spenta
        print("lampada spenta")
        client.publish(topic = "tettoia/lampada", payload = "spegnilampadatettoia")    
    else:
        print("cmd non gestito:"+str(comando))

def read_serial():
    stato=0
    sum=0
    cmd=0
    len=0
    numdati=0
    dati=bytearray(12)

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

def on_message(client, userdata, message):
    # print("Received message '" + str(message.payload) + "' on topic '" + message.topic + "' with QoS " + str(message.qos))
    msg=message.payload.decode()
    if msg=="accendilampadatettoia":
        Tx('c',0,0)
    elif msg=="spegnilampadatettoia":
        Tx('d',0,0)
    elif msg=="apricancello":
        Tx('C',0,0)
    elif msg=="apricancelloeporta":
        Tx('T',0,0)
    elif msg=="armaincasa":
        Tx('N',0,0)
    elif msg=="armanonincasa":
        Tx('P',0,0)
    elif msg=="disarma":
        Tx('O',0,0)
    else:
        print("comando mqtt non riconosciuto: " + str(message.payload))        


def on_log(client,userdata,level,buff):
    print("mqttlog" + buff)

def Tx(comando,datalen,dati):
    buf=bytearray(datalen+4)
    somma=ord(comando)+datalen
    buf[0]=ord('A')
    buf[1]=ord(comando)
    buf[2]=datalen
    for k in range(0,datalen):
        somma=somma+dati[k]
        buf[k+2]=dati[k]
    somma=somma & 255
    buf[datalen+3]=somma
    s.write(buf)
    print(buf)


client.username_pw_set("U1289$hr", "I8234%yu")
#client.tls_set("/Volumes/Hdd/Users/ac/lavori/SolarThermostat/certs/ca.crt")
client.tls_set("/home/andrea/HAServer/conf/mosquitto/ca.crt")
client.connect("ha.caveve.it",8883)
client.on_message = on_message
#client.on_log = on_log
client.loop_start()
client.publish(topic = "sta", payload = "485cli started") 
client.subscribe("485gateway")
t = threading.Thread(target=read_serial)
t.start()

while True:
    value = input("Comando:\n")
    value=value.split()
    if value[0]=="Q":
        devouscire=True
        s.close
        print("ciao")
        break
    elif value[0]=="T":
        #buf=b'\x41\x4C\x00\x4C'
        #print(buf)
        #s.write(buf)
        buf=bytes(0)
        Tx('L',0,buf)
    elif value[0]=="S" and len(value)==2:
        soglia=float(value[1])
        sd=int(1024*soglia/5)
        buff=bytearray(b'\x41Q\x03S')
        buff.append(sd & 255)
        buff.append(sd >> 8)
        somma=0
        l=len(buff)
        for x in range(1,l):
            somma=somma+buff[x]
        buff.append(somma & 255)
        print(buff)
        #for i in buff:
        s.write(buff)

client.loop_stop()
client.disconnect()

