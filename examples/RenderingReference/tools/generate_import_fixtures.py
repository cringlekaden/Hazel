"""Exact, original diagnostic GLBs. Generates source fixtures only; native tools own cooking."""
from pathlib import Path
import copy,hashlib,json,struct,zlib
ROOT=Path(__file__).resolve().parents[1]/'Assets/Reference/Import_v1'

def png(width,height,pixels):
    def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    rows=b''.join(b'\0'+pixels[y*width*4:(y+1)*width*4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')

def glb(doc,binary):
    doc=copy.deepcopy(doc);doc['buffers']=[{'byteLength':len(binary)}]
    text=json.dumps(doc,sort_keys=True,separators=(',',':'),ensure_ascii=True).encode();text+=b' '*((-len(text))%4)
    binary+=b'\0'*((-len(binary))%4)
    return b'glTF'+struct.pack('<II',2,28+len(text)+len(binary))+struct.pack('<I4s',len(text),b'JSON')+text+struct.pack('<I4s',len(binary),b'BIN\0')+binary

class Model:
    def __init__(self):self.doc={'asset':{'version':'2.0','generator':'Hazel RenderingReference Import_v1'},'bufferViews':[],'accessors':[]};self.binary=b''
    def view(self,data,stride=None):
        self.binary+=b'\0'*((-len(self.binary))%4);v={'buffer':0,'byteOffset':len(self.binary),'byteLength':len(data)}
        if stride:v['byteStride']=stride
        self.doc['bufferViews'].append(v);self.binary+=data;return len(self.doc['bufferViews'])-1
    def accessor(self,view,component,count,shape,offset=0,normalized=False,minimum=None,maximum=None):
        a={'bufferView':view,'componentType':component,'count':count,'type':shape,'byteOffset':offset}
        if normalized:a['normalized']=True
        if minimum is not None:a['min']=minimum;a['max']=maximum
        self.doc['accessors'].append(a);return len(self.doc['accessors'])-1
    def bytes(self):return glb(self.doc,self.binary)

def slots():
    m=Model();packed=b''.join(struct.pack('<6f',*p,0,0,1) for p in [(0,0,0),(1,0,0),(0,1,0),(-1,0,0)])
    view=m.view(packed,24);p=m.accessor(view,5126,4,'VEC3',minimum=[-1,0,0],maximum=[1,1,0]);n=m.accessor(view,5126,4,'VEC3',offset=12)
    uv=m.accessor(m.view(struct.pack('<8H',0,0,65535,0,0,65535,65535,0)),5123,4,'VEC2',normalized=True)
    color=m.accessor(m.view(bytes([255,255,255,255]*4)),5121,4,'VEC4',normalized=True)
    indices=m.accessor(m.view(struct.pack('<6H',0,1,2,0,2,3)),5123,6,'SCALAR')
    pv=m.view(struct.pack('<9f',1.5,0,0,2.5,0,0,1.5,1,0));p2=m.accessor(pv,5126,3,'VEC3',minimum=[1.5,0,0],maximum=[2.5,1,0]);i2=m.accessor(m.view(bytes([0,1,2])),5121,3,'SCALAR')
    base=png(2,2,bytes([255,0,0,63,0,255,0,128,0,0,255,192,255,255,255,255]));normal=png(2,2,bytes([128,128,255,255]*4))
    m.doc['images']=[{'name':'Asymmetric RGBA color','mimeType':'image/png','bufferView':m.view(base)},{'name':'Linear tangent normal','mimeType':'image/png','bufferView':m.view(normal)}]
    m.doc['samplers']=[{'minFilter':9987,'magFilter':9729,'wrapS':10497,'wrapT':33071}];m.doc['textures']=[{'source':0,'sampler':0},{'source':1,'sampler':0}]
    m.doc['materials']=[{'name':'Masked color and linear normal','pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicFactor':0,'roughnessFactor':0.5},'normalTexture':{'index':1,'scale':1},'alphaMode':'MASK','alphaCutoff':0.5},{'name':'Unlit known linear swatch','pbrMetallicRoughness':{'baseColorFactor':[0.21404114,0.5,1,1]},'extensions':{'KHR_materials_unlit':{}}}]
    m.doc['extensionsUsed']=['KHR_materials_unlit'];m.doc['meshes']=[{'name':'Two slots with tangent mirror boundary','primitives':[{'attributes':{'POSITION':p,'NORMAL':n,'TEXCOORD_0':uv,'COLOR_0':color},'indices':indices,'material':0},{'attributes':{'POSITION':p2},'indices':i2,'material':1}]}]
    m.doc['nodes']=[{'name':'Instance identity','mesh':0},{'name':'Instance rotated','mesh':0,'translation':[0,0,2],'rotation':[0,0.382683432365,0,0.923879532511]},{'name':'Instance mirrored and nonuniform','mesh':0,'translation':[4,0,0],'scale':[-1,2,1]}];m.doc['scenes']=[{'name':'Slots and repeated instances','nodes':[0,1,2]}];m.doc['scene']=0;return m

def provided():
    m=Model();view=m.view(b''.join(struct.pack('<10f',*p,0,0,1,1,0,0,1) for p in [(0,0,0),(1,0,0),(0,1,0)]),40)
    p=m.accessor(view,5126,3,'VEC3',minimum=[0,0,0],maximum=[1,1,0]);n=m.accessor(view,5126,3,'VEC3',offset=12);t=m.accessor(view,5126,3,'VEC4',offset=24)
    uv=m.accessor(m.view(struct.pack('<6f',0,0,1,0,0,1)),5126,3,'VEC2');c=m.accessor(m.view(struct.pack('<12f',1,0,0,1,0,1,0,.5,0,0,1,1)),5126,3,'VEC4');i=m.accessor(m.view(struct.pack('<3I',0,1,2)),5125,3,'SCALAR')
    m.doc.update(meshes=[{'name':'Provided tangent and float vertex color','primitives':[{'attributes':{'POSITION':p,'NORMAL':n,'TANGENT':t,'TEXCOORD_0':uv,'COLOR_0':c},'indices':i}]}],nodes=[{'name':'Exact reflected affine matrix','mesh':0,'matrix':[-1,0,0,0,0,1,0,0,0,0,1,0,3,0,0,1]}],scenes=[{'nodes':[0]}],scene=0);return m

def main():
    root=ROOT/'Sources';root.mkdir(parents=True,exist_ok=True);a=slots();b=provided();cases={}
    def put(name,data,accepted,meaning,diagnostic=''):
        (root/name).write_bytes(data);cases[name]={'sha256':hashlib.sha256(data).hexdigest(),'accepted':accepted,'meaning':meaning}
        if diagnostic:cases[name]['diagnostic']=diagnostic
    put('SlotsAndInstances.glb',a.bytes(),True,'Two stable slots, repeated instances, signed nonuniform TRS, normalized UV/color, stride 24, U8/U16 indices, Mikk corner splits, embedded color/data PNG and Mask/Unlit semantics')
    put('ProvidedTangents.glb',b.bytes(),True,'FLOAT normal/tangent/UV/color, U32 indices, default glTF material and exact reflected affine matrix')
    put('Truncated.glb',a.bytes()[:33],False,'Truncated chunk framing','complete GLB 2')
    def bad(name,mutate,diagnostic):
        m=copy.deepcopy(a);mutate(m);put(name,m.bytes(),False,name.removesuffix('.glb'),diagnostic)
    bad('ExcessiveVertexCount.glb',lambda m:m.doc['accessors'][0].update(count=1000001),'count')
    bad('OverflowingOffset.glb',lambda m:m.doc['bufferViews'][0].update(byteOffset=18446744073709551615),'bounded nonnegative integer')
    bad('ExternalURI.glb',lambda m:m.doc['images'][0].update(uri='external.png'),'uri')
    bad('AlphaBlend.glb',lambda m:m.doc['materials'][0].update(alphaMode='BLEND'),'BLEND')
    bad('MaterialExtension.glb',lambda m:m.doc['materials'][0].update(extensions={'KHR_materials_transmission':{'transmissionFactor':1}}),'KHR_materials_transmission')
    bad('ExtraUV.glb',lambda m:m.doc['meshes'][0]['primitives'][0]['attributes'].update(TEXCOORD_1=2),'TEXCOORD_1')
    bad('SparseAccessor.glb',lambda m:m.doc['accessors'][0].update(sparse={'count':1}),'sparse')
    def shear(m):m.doc['nodes'][0].update(matrix=[1,0,0,0,.5,1,0,0,0,0,1,0,0,0,0,1])
    bad('ShearedNode.glb',shear,'shear')
    meta={'version':1,'name':'Import_v1','acceptance':'CPU import/cooking/metadata only; no mesh/PBR rendering acceptance','coordinates':'right handed, +Y up, metres; UV/image rows top-left','license':'Original generated diagnostic content, Apache-2.0 (repository LICENSE)','generator':'tools/generate_import_fixtures.py; Python standard library only','cases':cases}
    (ROOT/'sources.json').write_text(json.dumps(meta,indent=2,sort_keys=True)+'\n')
if __name__=='__main__':main()
