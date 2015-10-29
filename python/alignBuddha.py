import numpy as np
from scipy.linalg import inv
from js.data.plyParse import PlyParse
import mayavi.mlab as mlab
import os.path, re
import subprocess as subp
from js.geometry.rotations import Quaternion
from js.utils.plot.colors import colorScheme

cfgBuddha = {"lambdaS3": 60, "lambdaR3": 0.001}

scans = ['../data/happy_side/happySideRight_0.ply',
  '../data/happy_side/happySideRight_24.ply',
  '../data/happy_side/happySideRight_48.ply',
  '../data/happy_side/happySideRight_72.ply',
  '../data/happy_side/happySideRight_96.ply',
  '../data/happy_side/happySideRight_120.ply',
  '../data/happy_side/happySideRight_144.ply',
  '../data/happy_side/happySideRight_168.ply',
  '../data/happy_side/happySideRight_192.ply']
#  '../data/happy_side/happySideRight_216.ply']
#  '../data/happy_side/happySideRight_240.ply',
#  '../data/happy_side/happySideRight_264.ply',
#  '../data/happy_side/happySideRight_288.ply',
#  '../data/happy_side/happySideRight_312.ply',
#  '../data/happy_side/happySideRight_336.ply']

scans = ['../data/happy_side/happySideRight_48.ply',
  '../data/happy_side/happySideRight_72.ply']
scans = ['../data/happy_side/happySideRight_24.ply',
  '../data/happy_side/happySideRight_48.ply']

cfg = cfgBuddha
loadCached = True
stopToShow = True

pattern = "happySideRight_[0-9]+_angle_90_translation_0.3.ply$"
scans = []
for root, dirs, files in os.walk("../data/happy_side_rnd/"):
  for f in files:
    if re.search(pattern, f):
      scans.append(os.path.join(root, f))

scans = sorted(scans, key=lambda f: 
  int(re.sub("_angle_90_translation_0.3.ply","",re.sub("happySideRight_","",os.path.split(f)[1]))))

print scans
colors = colorScheme("label")

W_T_B = np.eye(4)
for i in range(1,len(scans)):
  scanApath = scans[i-1]
  scanBpath = scans[i]
  nameA = os.path.splitext(os.path.split(scanApath)[1])[0]
  nameB = os.path.splitext(os.path.split(scanBpath)[1])[0]
  transformationPath = '{}_{}.csv'.format(nameA, nameB)

  if i == 1:
    plyA = PlyParse();
    plyA.parse(scanApath)
    pcA = plyA.getPc()
    figm = mlab.figure()
    mlab.points3d(pcA[:,0], pcA[:,1], pcA[:,2], mode="point",
        color=colors[0])

  if loadCached and os.path.isfile(transformationPath):
    print "found transformation file and using it "+transformationPath
  else:
    args = ['../pod-build/bin/dpvMFoptRotPly', 
        '-a {}'.format(scanApath), 
        '-b {}'.format(scanBpath), 
        '-l {}'.format(cfg["lambdaS3"]),
        '-t {}'.format(cfg["lambdaR3"]),
        '-o {}_{}'.format(nameA, nameB)
        ]
    print " ".join(args)
    err = subp.call(" ".join(args), shell=True)

  with open(transformationPath) as f:
    f.readline()
    qt = np.loadtxt(f)
    q = qt[:4];
    print 'q', q
    t = qt[4:];
    print 't', t
    q = Quaternion(w=q[0], x=q[1], y=q[2], z=q[3])
    R = q.toRot().R
    print "R", R
    A_T_B = np.eye(4)
    A_T_B[:3,:3] = R.T
    A_T_B[:3,3] = -R.T.dot(t)
    W_T_B = W_T_B.dot(A_T_B)

  plyB = PlyParse();
  plyB.parse(scanBpath)
  pcB = plyB.getPc()

  R = W_T_B[:3,:3]
  t = W_T_B[:3,3]
  pcB = (R.dot(pcB.T)).T + t
  
  mlab.points3d(pcB[:,0], pcB[:,1], pcB[:,2], mode="point",
        color=colors[i%len(colors)])
  if stopToShow:
    mlab.show(stop=True)
mlab.show(stop=True)