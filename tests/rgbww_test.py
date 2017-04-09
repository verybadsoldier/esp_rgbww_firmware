'''
Created on 02.04.2017

@author: Robin
'''
import unittest
import requests
import json
import time

#host = "sz-led-wall"
host = "wz-led-tv"

jsonTempl = u'''{{
  "q":"{queue}",
  "hsv":{{"ct":2700,"v":"{val}","h":"{hue}","s":"{sat}"}},
  "d":1,
  "t":{time},
  "cmd":"{cmd}"
}}
'''

jsonTemplChannels = u'''{{channels:[{channels}]}}'''

jsonTemplSolid = u'''{{
  "hsv":{{"ct":2700,"v":"{v}","h":"{h}","s":"{s}"}},
  "d":1,
  "cmd":"solid"
}}
'''

def do_post(queryParams, post_data):
    r = requests.request(u"POST", u"http://{}/{}".format(host, queryParams), data=post_data)
    if (r.status_code != 200):
        raise Exception("Error")

def get_color():
    r = requests.request(u"GET", u"http://{}/color".format(host))
    return json.loads(r.text)

def get_hue():
    return get_color()['hsv']['h']

def get_sat():
    return get_color()['hsv']['s']

def get_val():
    return get_color()['hsv']['v']

def rgbww_set(h, s, v):
    post_data = jsonTemplSolid.format(h=h, s=s, v=v)
    do_post(u"color", post_data)

def set_hue_fade(hue, ramp, sat=100, val=100, queuePolicy="back"):
    ts = time.time()
    print u"Setting hue {} with ramp {} s".format(hue, unicode(ramp))
    post_data = jsonTempl.format(hue=hue, val=val, sat=sat, time=unicode(int(ramp * 1000)), queue=queuePolicy, cmd="fade")
    r = requests.request(u"POST", u"http://{}/color".format(host), data=post_data)
    if (r.status_code != 200):
        raise Exception(r.content)
    return ts

def set_channel_cmd(cmd, channels="'h','s','v'"):
    ts = time.time()
    print u"Setting " + cmd
    post_data = jsonTemplChannels.format(channels=channels)
    r = requests.request(u"POST", u"http://{}/{}".format(host, cmd), data=post_data)
    if (r.status_code != 200):
        raise Exception("Error")
    return ts

def set_and_print(hue, ramp):
    ts = set_hue_fade(hue, ramp)
    
    while True:
        color_now = get_color()
        cur_hue = color_now['hsv']['h']

        ts_now = time.time()
        print("Current hue: {} - Time since ramp start: {:.2f}".format(cur_hue, ts_now - ts))

        if (abs(cur_hue - hue) < 0.5):
            break
        if (ts_now - ts > ramp):
            raise Exception("!!!!!!!!! Ramp taking too long!")
        time.sleep(3.0)
        
        
class RgbwwTest(unittest.TestCase):
    
    def setUp(self):
        rgbww_set(0, 100, 100)
        
    def testSimpleFade(self):
        new_hue = 120
        ramp = 10
        set_hue_fade(new_hue, ramp)
        time.sleep(ramp)
        cur_hue = get_hue()
        
        self.assertAlmostEqual(cur_hue, new_hue, delta=0.5)

    def testQueueBack(self):
        ramp = 10
        set_hue_fade(120, ramp)
        set_hue_fade(170, ramp)
        time.sleep(2 * ramp)
        
        self.assertAlmostEqual(get_hue(), 170, delta=0.5)
        
    def testQueueFrontReset(self):
        ramp = 12
        delta = 0.8
        set_hue_fade(120, ramp)
        time.sleep(6)
        hue1 = get_hue() # 60
        set_hue_fade(30, 12, queuePolicy="front_reset")
        time.sleep(6)
        hue2 = get_hue() # 45
        time.sleep(6)
        hue3 = get_hue() # 30
        time.sleep(6)
        hue4 = get_hue() # 75

        self.assertAlmostEqual(hue1, 60, delta=delta)
        self.assertAlmostEqual(hue2, 45, delta=delta)
        self.assertAlmostEqual(hue3, 30, delta=delta)
        self.assertAlmostEqual(hue4, 75, delta=delta)
        
    def testQueueFront(self):
        ramp = 12
        delta = 2.0
        set_hue_fade(120, ramp)
        time.sleep(6)
        hue1 = get_hue() # 60
        set_hue_fade(30, 12, queuePolicy="front")
        time.sleep(6)
        hue2 = get_hue() # 45
        time.sleep(6)
        hue3 = get_hue() # 60
        time.sleep(3)
        hue4 = get_hue() # 60

        self.assertAlmostEqual(hue1, 60, delta=delta)
        self.assertAlmostEqual(hue2, 45, delta=delta)
        self.assertAlmostEqual(hue3, 60, delta=delta)
        self.assertAlmostEqual(hue4, 90, delta=delta)

    def testRelativePlus(self):
        ramp = 3
        delta = 0.8
        set_hue_fade("+10", ramp)
        time.sleep(3)
        hue1 = get_hue() # 60

        self.assertAlmostEqual(hue1, 10, delta=delta)
  
    def testRelativePlus_CircleTop(self):
        set_hue_fade("350", 0)
        ramp = 3
        set_hue_fade("+20", ramp)
        time.sleep(3)
        hue1 = get_hue() # 60

        delta = 0.8
        self.assertAlmostEqual(hue1, 10, delta=delta)
              
    def testRelativePlus2(self):
        ramp = 3
        delta = 0.8
        set_hue_fade("+10", ramp)
        set_hue_fade("+10", ramp)
        time.sleep(3)
        hue1 = get_hue()
        time.sleep(3)
        hue2 = get_hue()

        self.assertAlmostEqual(hue1, 10, delta=delta)  
        self.assertAlmostEqual(hue2, 20, delta=delta)
        
    def testRelativeMinus(self):
        set_hue_fade("100", 0)
        ramp = 3
        set_hue_fade("-10", ramp)
        time.sleep(ramp)
        hue1 = get_hue()

        delta = 0.8
        self.assertAlmostEqual(hue1, 90, delta=delta)
        
    def testRelativeMinus_CircleBottom(self):
        set_hue_fade("100", 0)
        ramp = 3
        set_hue_fade("-150", ramp)
        time.sleep(3)
        hue1 = get_hue()

        delta = 0.8
        self.assertAlmostEqual(hue1, 310, delta=delta)
    
    def testPauseAll(self):
        set_hue_fade("100", val=50, sat=50, ramp=10)
        time.sleep(5)
        set_channel_cmd("pause")
        time.sleep(5)
        hue1 = get_hue()
        sat1 = get_sat()
        val1 = get_val()
        set_channel_cmd("continue")
        time.sleep(5)
        hue2 = get_hue()
        sat2 = get_sat()
        val2 = get_val()
        
        delta = 0.8
        self.assertAlmostEqual(hue1, 50, delta=delta)  
        self.assertAlmostEqual(sat1, 75, delta=delta)  
        self.assertAlmostEqual(val1, 75, delta=delta)  
        self.assertAlmostEqual(hue2, 100, delta=delta)  
        self.assertAlmostEqual(sat2, 50, delta=delta)  
        self.assertAlmostEqual(val2, 50, delta=delta)  

    def testPauseChannel(self):
        set_hue_fade("100", val=50, sat=50, ramp=10)
        time.sleep(5)
        set_channel_cmd("pause", 'h')
        time.sleep(5)
        hue1 = get_hue()
        sat1 = get_sat()
        val1 = get_val()
        set_channel_cmd("continue", 'h')
        time.sleep(5)
        hue2 = get_hue()
        sat2 = get_sat()
        val2 = get_val()
        
        delta = 0.8
        self.assertAlmostEqual(hue1, 50, delta=delta)  
        self.assertAlmostEqual(sat1, 50, delta=delta)  
        self.assertAlmostEqual(val1, 50, delta=delta)  
        self.assertAlmostEqual(hue2, 100, delta=delta)  
        self.assertAlmostEqual(sat2, 50, delta=delta)  
        self.assertAlmostEqual(val2, 50, delta=delta)  
        
if __name__ == "__main__":
    #import sys;sys.argv = ['', 'Test.testName']
    unittest.main()