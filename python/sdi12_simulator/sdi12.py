from machine import Pin, Timer
import time, random

class SDI12_Sensor:
    def __init__(self, addr):
        self.addr = addr
        
    #both funcs for both sensors
    def identify(self):
        raise NotImplementedError
    def handle(self, cmd):
        raise NotImplementedError
            
#the following classes represent sensors and return messages for id and for data    
#only needs aXR
class Solyx14(SDI12_Sensor):
    def identify(self):
        return f"{self.addr}14METER   SLYX14400SX14000000{self.addr}01\r\n"
    def handle(self, cmd):
        if cmd == f"{self.addr}XR0!":
            #randomise vals a bit and send str
            epsr = 15.0 + random.uniform(-1.0, 1.0)
            temp = 21.0 + random.uniform(-0.5, 0.5)
            bulk_ec = 0.15 + random.uniform(-0.1, 0.1)
            return f"{self.addr}+{epsr:.3f}+{temp:.2f}+{bulk_ec:.3f}\r\n"

#will handle aM and aD0 comm
class Solinst(SDI12_Sensor):
    def __init__(self, addr):
        super().__init__(addr)
        self.measuring = False
    def identify(self):
        return f"{self.addr}13SOLINST M10/C XD11.006 1090717\r\n"
    def handle(self, cmd):
        if cmd == f"{self.addr}M!":
            self.measuring = True
            return f"{self.addr}0012\r\n" #ready in 0, 2vals
        if cmd == f"{self.addr}D0!" and self.measuring:
            self.measuring = False
            temp = 20.0 + random.uniform(-0.3, 0.3)
            level = 1.023 + random.uniform(-0.005, 0.005)
            return f"{self.addr}+{temp:.3f}+{level:.4f}\r\n"

        