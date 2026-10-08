"""Original example pixel art. Optional regeneration: Python + Pillow (12.3 tested).
No external assets. Fixed palettes and a local RNG make output reproducible.
"""
from pathlib import Path
import random
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parents[2]

def canvas(game, name, size, paint, background=(0,0,0,0)):
    image=Image.new('RGBA',size,background);paint(ImageDraw.Draw(image),size)
    path=ROOT/'examples'/game/'Assets/Textures'/f'{name}.png'
    path.parent.mkdir(parents=True,exist_ok=True);image.save(path)

pine='#244c45';leaf='#47785a';light='#9fbe75';cream='#fff0c9'
def grass(d,s):
    d.rectangle((0,0,63,63),fill='#769b63')
    rng=random.Random(17)
    for i in range(65):
        x,y=rng.randrange(60),rng.randrange(60)
        d.line((x,y,x+1,y-3),fill=rng.choice(['#86ac70','#608a59','#8eb177']),width=1)
    for x,y in [(12,13),(46,36),(24,51)]:
        d.rectangle((x,y,x+2,y+2),fill='#d9df91')
canvas('MeadowRun','Grass',(64,64),grass)
def path(d,s):
    d.rectangle((0,0,63,63),fill='#d9c695')
    rng=random.Random(22)
    for i in range(60):
        x,y=rng.randrange(64),rng.randrange(64);d.rectangle((x,y,x+2,y+1),fill=rng.choice(['#c7b486','#e8d5a5']))
canvas('MeadowRun','Path',(64,64),path)
def tree(d,s):
    d.ellipse((8,77,58,94),fill='#315d47');d.rectangle((28,48,37,86),fill='#71573d')
    d.rectangle((30,50,32,83),fill='#a18452')
    for box,col in [((5,22,58,74),pine),((1,23,36,62),leaf),((26,14,62,62),leaf),((12,4,48,49),leaf),((12,9,38,36),light),((7,31,23,49),'#80a767'),((34,22,53,40),'#679965')]:d.ellipse(box,fill=col)
    d.rectangle((19,15,25,19),fill='#bdd08d');d.rectangle((38,42,42,46),fill='#bfd18b')
canvas('MeadowRun','Tree',(64,96),tree)
def rock(d,s):
    d.ellipse((3,37,62,62),fill='#3c6651');d.polygon([(4,42),(14,16),(37,6),(55,21),(61,48),(44,56),(17,53)],fill='#879991')
    d.polygon([(14,16),(37,6),(49,22),(25,27),(4,42)],fill='#b7c4ae');d.line((25,27,37,42,44,56),fill='#5e7d77',width=3)
canvas('MeadowRun','Rock',(64,64),rock)
def player(d,s):
    d.ellipse((12,47,51,59),fill='#375e48');d.rectangle((20,39,27,53),fill='#243f43');d.rectangle((36,39,43,53),fill='#243f43')
    d.rounded_rectangle((15,22,49,46),radius=6,fill='#e0985b');d.rectangle((21,24,43,33),fill='#f4c274')
    d.rectangle((23,34,41,42),fill='#517868');d.rectangle((25,34,39,37),fill='#8eb49c')
    d.rounded_rectangle((19,9,44,28),radius=6,fill='#f4d7a5');d.rectangle((25,20,28,23),fill=pine);d.rectangle((37,20,40,23),fill=pine)
    d.ellipse((15,3,49,18),fill='#668451');d.rectangle((12,12,52,17),fill='#acc17a');d.rectangle((39,5,43,10),fill='#ffe6a0')
canvas('MeadowRun','Explorer',(64,64),player)
def seed(d,s):
    d.ellipse((8,23,42,45),fill='#548055');d.polygon([(25,4),(16,16),(14,30),(25,40),(36,30),(34,16)],fill='#d89348')
    d.polygon([(25,8),(20,18),(19,28),(25,34),(31,28),(30,18)],fill='#ffdf80');d.rectangle((24,14,27,25),fill='#fff7cc')
    d.line((25,9,24,3,32,2),fill=pine,width=3);d.polygon([(28,3),(38,2),(33,8)],fill=leaf)
canvas('MeadowRun','Seed',(48,48),seed)
def pond(d,s):
    d.ellipse((1,3,94,92),fill='#506e55');d.ellipse((5,7,90,88),fill='#91b8a1');d.ellipse((10,12,85,82),fill='#478c98')
    for x,y in [(20,36),(51,55),(40,23)]:d.line((x,y,x+21,y),fill='#96d1c1',width=2)
    d.ellipse((58,16,78,33),fill='#bad38b');d.rectangle((65,23,68,28),fill='#eff0b8')
canvas('MeadowRun','Pond',(96,96),pond)
def gate(d,s):
    d.rectangle((7,22,18,79),fill='#725e42');d.rectangle((62,22,73,79),fill='#725e42');d.rectangle((11,27,15,73),fill='#b7a46e');d.rectangle((66,27,70,73),fill='#b7a46e')
    d.rounded_rectangle((2,2,78,31),radius=5,fill=pine);d.rounded_rectangle((6,5,74,27),radius=3,fill=leaf)
    d.polygon([(29,14),(41,8),(52,14),(43,14),(43,23),(37,23),(37,14)],fill=cream)
    d.rectangle((22,59,58,68),fill='#d7bd76');d.rectangle((23,71,57,77),fill='#e9d396')
canvas('MeadowRun','Exit',(80,80),gate)
def button(d,s):
    d.rounded_rectangle((1,3,127,45),radius=9,fill='#183e39');d.rounded_rectangle((2,1,126,41),radius=9,fill='#bdcd8a');d.rounded_rectangle((5,4,123,38),radius=7,fill='#416956');d.line((14,8,111,8),fill='#719c73',width=2)
canvas('MeadowRun','Button',(128,48),button)
# Skybound: dusk indigo, coral architecture, mint wind and warm stars.
def sky(d,s):
    for y in range(1024):
        t=y/1023;d.line((0,y,1023,y),fill=(int(37+45*t),int(53+37*t),int(98+38*t),255))
    rng=random.Random(4)
    for i in range(350):
        x,y=rng.randrange(1024),rng.randrange(700);d.point((x,y),fill='#c3cdda')
canvas('Skybound','Sky',(1024,1024),sky)
def cloud(d,s):
    for box in [(1,18,57,49),(21,4,79,47),(57,14,120,48)]:d.ellipse(box,fill='#a2a5c9')
    d.rectangle((20,33,108,48),fill='#a2a5c9');d.line((18,39,92,39),fill='#d0c9dd',width=2)
canvas('Skybound','Cloud',(128,64),cloud)
def mountain(d,s):
    d.polygon([(0,80),(44,20),(70,51),(112,4),(153,48),(180,14),(255,80),(255,127),(0,127)],fill='#626e96')
    d.polygon([(92,29),(112,4),(130,26),(115,21),(106,30)],fill='#afb4c9')
    d.polygon([(0,105),(60,55),(105,96),(158,43),(223,92),(255,76),(255,127),(0,127)],fill='#475b83')
canvas('Skybound','Mountains',(256,128),mountain)
def moth(d,s):
    d.ellipse((2,22,35,48),fill='#8dd8d0');d.ellipse((31,20,62,46),fill='#8dd8d0');d.ellipse((4,27,26,40),fill='#d6edce');d.ellipse((39,25,59,39),fill='#d6edce')
    d.polygon([(17,24),(47,24),(53,52),(32,47),(12,53)],fill='#b080be');d.rounded_rectangle((20,8,47,38),radius=8,fill='#614d91')
    d.polygon([(33,13),(44,25),(33,35),(23,25)],fill='#fff0bf');d.rectangle((28,23,30,26),fill='#50437e');d.rectangle((37,23,39,26),fill='#50437e');d.rectangle((29,8,35,12),fill='#efa87e')
canvas('Skybound','Wisp',(64,64),moth)
def column(d,s):
    d.rectangle((1,0,62,511),fill='#635476');d.rectangle((5,0,58,511),fill='#b87688');d.rectangle((9,0,17,511),fill='#dea397')
    for y in range(8,512,24):
        d.line((19,y,56,y),fill='#8d627e',width=2);d.rectangle((38,y+5,42,y+13),fill='#d7a298')
    d.rectangle((0,500,63,511),fill='#9dc9be');d.rectangle((3,502,60,506),fill='#d6dfc2')
canvas('Skybound','Column',(64,512),column)
def ground(d,s):
    d.rectangle((0,0,1023,63),fill='#48567a');d.rectangle((0,0,1023,8),fill='#a6cbb5');d.rectangle((0,9,1023,14),fill='#6f969d')
    for x in range(0,1024,32):d.polygon([(x,29),(x+8,21),(x+15,30),(x+8,38)],fill='#657593')
canvas('Skybound','Ground',(1024,64),ground)
def skybutton(d,s):
    d.rounded_rectangle((1,3,127,45),radius=9,fill='#383d66');d.rounded_rectangle((2,1,126,41),radius=9,fill='#ffddaa');d.rounded_rectangle((5,4,123,38),radius=7,fill='#af718f');d.line((14,8,111,8),fill='#dda09e',width=2)
canvas('Skybound','Button',(128,48),skybutton)
def panel(d,s):
    d.rounded_rectangle((1,1,159,99),radius=10,fill='#d6cfbe');d.rounded_rectangle((4,4,156,96),radius=8,fill='#343e65');d.line((17,11,143,11),fill='#667093',width=2)
canvas('Skybound','Panel',(160,100),panel)
