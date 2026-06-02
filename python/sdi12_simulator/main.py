from machine import UART, Pin                                                   
import time, random                                                               
from sdi12 import Solyx14, Solinst
             
def add_even_parity(ch):                                                          
    ch &= 0x7F                                                                    
    return ch | ((bin(ch).count('1') % 2) << 7)
    
class SDI12Bus:                                                                 
    def __init__(self, uart_id, tx_pin, rx_pin, break_pin):
        #not the 7E1, but adding even parity separately and sending 8bits
        self.uart = UART(uart_id, baudrate=1200, bits=8, parity=None, stop=1, tx=tx_pin, rx=rx_pin)                            
        self.break_gpio = Pin(break_pin, Pin.IN)                                  
                                                                                
    def wait_for_break(self):
        #wait for High and then Low
        while self.break_gpio.value() == 0:                                       
          pass
        t = time.ticks_ms()                                                       
        while self.break_gpio.value() == 1:                                     
          pass
        #from sdi12.c msleep 15 after sending br
        return time.ticks_diff(time.ticks_ms(), t) >= 15
                                                                                
    def read_cmd(self):
        time.sleep_ms(80)
        #print all
        print(self.uart.any())
        data = self.uart.read()
        if data:
            stripped = bytes([byte & 0x7F for byte in data])
            print("raw cmd:", stripped)
            #look for ! and then read command before that
            if b'!' in stripped:
                idx = stripped.index(b'!')
                return stripped[:idx+1].decode()
        return None
                                                                                    
    def respond(self, text):                                                    
        time.sleep_ms(10)
        for ch in text:                                                           
            self.uart.write(bytes([add_even_parity(ord(ch))]))                    
                                                                                    
sensors = [Solyx14('0'), Solyx14('1'), Solinst('2')]                              
bus = SDI12Bus(uart_id=1, tx_pin=4, rx_pin=5, break_pin=2)

bus.respond(sensors[2].identify())  
time.sleep_ms(10)
print("UART from pico:", bus.uart.read()) 
                                        
while True:        
    if bus.wait_for_break():
        print("Command received")
        cmd = bus.read_cmd()
        print("cmd:", cmd)  
        if cmd:                                                                                                                                        
            for s in sensors:
                if cmd.startswith(s.addr + 'I'):
                    #echo back command and then data
                    bus.respond(cmd)
                    bus.respond(s.identify())                                             
                    break
                resp = s.handle(cmd)                                                      
                if resp:
                    bus.respond(cmd)
                    bus.respond(resp)
                    break           
