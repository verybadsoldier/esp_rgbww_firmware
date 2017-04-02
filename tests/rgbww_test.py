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
  "q":"{2}",
  "hsv":{{"ct":2700,"v":"100","h":"{0}","s":"100"}},
  "d":1,
  "t":{1},
  "cmd":"{3}"
}}
'''

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

def rgbww_set(h, s, v):
    post_data = jsonTemplSolid.format(h=h, s=s, v=v)
    do_post(u"color?mode=HSV", post_data)

def set_hue_fade(hue, ramp, queuePolicy = "back"):
    ts = time.time()
    print u"Setting hue {} with ramp {} s".format(hue, unicode(ramp))
    post_data = jsonTempl.format(hue, unicode(int(ramp * 1000)), queuePolicy, "fade")
    r = requests.request(u"POST", u"http://{}/color?mode=HSV".format(host), data=post_data)
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
        delta = 0.8
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
        
if __name__ == "__main__":
    #import sys;sys.argv = ['', 'Test.testName']
    unittest.main()