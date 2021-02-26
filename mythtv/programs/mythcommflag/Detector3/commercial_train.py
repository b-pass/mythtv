#!/usr/bin/env python3

import sys
import re
import random
import datetime
import os

if len(sys.argv) < 2:
    print(sys.argv[0] + " <training_files...> [-t <test_file>]")
    print("Where test_file is used only for measuring accuracy and not for training.")
    print("File format:")
    print('''
Truthiness Start Frame-Frame Len Pos Scenes Logo Formats Audio

Truthiness = is this a commercial (for training)
Start = Offset in seconds from start of show (not used)
Frame-Frame = Range of frame numbers for this segment, used in output
Len = Length in seconds of segment
Scenes = Scene changes
Logo = Logo detection (as percent of frames)
Formats = Number of format changes
Sizes = Number of size changes
Audio = Total and peak audio for every channel

Truthiness is a raw score, where < 0 is a commercial and >= 0 
is a show segment

# comment lines and blank lines are legal and ignored
    ''')
    sys.exit(1)

print("Loading...")
def normalize(when,parts):
    when = float(when - int(when / 3600)*3600) / 3600.0
    parts[0] = float(parts[0])
    parts[1] = float(parts[1]) / parts[0]
    parts[2] = float(parts[2])
    parts[3] = float(parts[3]) / parts[0]
    parts[4] = float(parts[4]) / parts[0]
    for c in range(8):
        parts[5+c*2] = float(parts[5+c*2]) / parts[0]
        parts[6+c*2] = float(parts[6+c*2]) / 32767.0
    return [when] + parts
    
def load(filename):
    info = []
    data = []
    answers = []
    
    last_last_last_ans = 0.0
    last_last_ans = 0.0
    last_ans = 0.0
    ctime = 0
    logo = 0.0
    nologo = 0.0
    for line in open(filename, 'r'):
        line = line.strip()
        if not line or line[0] == '#':
            continue
        
        parts = line.split()
        if len(parts) < 11:
            print(filename + ": Ignored bad line: " + line)
            continue
        
        if parts[2][-1] == '-':
            parts[2] += parts[3]
            del parts[3]
        
        score = int(parts[0])
        if score == 0:
            continue
        elif score < 0:
            answers += [[1.0]]
        else:
            answers += [[0.0]]
        
        info += [parts[2]]

        sec = int(parts[1].split(':')[0])*60 + float(parts[1].split(':')[1])
        #parts[4] = (sec % 3600) / 15.0 / (3600.0/15.0)
        
        #f = parts[2].split('-', 2)
        #flen = (int(f[1].strip()) - int(f[0].strip()))
        #lp = max(0.0,min(float(parts[6]),1.0))
        #logo += lp * flen
        #nologo += (1.0 - lp) * flen
        
        d = normalize(sec, parts[3:])
        d.append(last_ans)
        #d.append(last_last_ans)
        #d.append(last_last_last_ans)
        #d.append(min(ctime/1200.0, 1.0))
        data.append(d)
        
        last_last_last_ans = last_last_ans
        last_last_ans = last_ans
        
        if score <= 0:
            last_ans = 1.0
            ctime = 0
        else:
            last_ans = 0.0
            ctime += sec
        
    #for d in data:
    #    if logo < nologo:
    #        d[6] = 1.0
    #        d += [0.0]
    #    else:
    #        d += [1.0]
    
    return (info, data, answers)

input_data = {}
for f in sys.argv[1:]:
    if f == "-t":
        break
    if not os.path.isfile(f):
        print("Not a file",f)
        continue
    (i, d, a) = load(f)
    input_data[f] = (d, a)

test_data = {}
tdx = 0
if "-t" in sys.argv:
    i = len(sys.argv) - 1
    while i > 0:
        f = sys.argv[i]
        i -= 1
        if f == "-t":
            break
        if not os.path.isfile(f):
            try:
                tdx = int(f)
            except:
                print("Not a file",f)
            continue
        if f not in input_data:
            (ix, d, a) = load(f)
            input_data[f] = (d, a)
        test_data[f] = input_data[f]
        del input_data[f]

if not test_data:
    want = len(input_data)/5
    k = [k for k in input_data.keys()]
    random.shuffle(k)
    while len(test_data) < want:
        f = k[0]
        del k[0]
        test_data[f] = input_data[f]
        del input_data[f]

num_data_cols = len(next (iter (input_data.values()))[0][0])
all_input_data = []
all_input_answers = []
all_test_data = []
all_test_answers = []
for (f, d) in input_data.items():
    all_input_data += d[0]
    all_input_answers += d[1]
if not tdx:
    for (f, d) in test_data.items():
        all_test_data += d[0]
        all_test_answers += d[1]

print("Starting TF...")

import numpy as np
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers

import signal
class GracefulStop(keras.callbacks.Callback):
	def __init__(self):
		super(keras.callbacks.Callback, self).__init__()
		self._stop = False
		def handler(signum, frame):
			self._stop = True
			print("\nStopping!\n")
		signal.signal(signal.SIGINT, handler)

	def on_epoch_end(self, epoch, logs={}):
		if self._stop:
			print("\nGraceful stop\n")
			self.model.stop_training = True

DROPOUT = 0.1

name = str(num_data_cols)
inputs = keras.Input(shape=(num_data_cols))
x = inputs
x = layers.BatchNormalization()(x)
if DROPOUT:
    x = layers.Dropout(DROPOUT)(x)
    name += 'd' + str(DROPOUT)
#h = num_data_cols
#for i in range(10):
#	x = layers.Dense(h, activation='tanh')(x)
#	name += '_' + str(h)
#	h = int(h/2)
#	if h < 2:
#		break
#for h in [21, 17, 21]:
#[45,60,35]:
#for h in [64,64,64]:
for h in [64]*10:
	x = layers.Dense(h, activation='tanh')(x)
	name += '_' + str(h)
outputs = layers.Dense(1, activation='sigmoid')(x)
model = keras.Model(inputs, outputs)
model.summary()

model.compile(optimizer="adam", loss="binary_crossentropy", metrics=['binary_accuracy', 'mean_squared_error'])

batch_size=12800
train_dataset = tf.data.Dataset.from_tensor_slices((all_input_data, all_input_answers)).batch(batch_size)
test_dataset = tf.data.Dataset.from_tensor_slices((all_test_data, all_test_answers)).batch(batch_size)
callbacks = [
	GracefulStop(),
    # keras.callbacks.ModelCheckpoint('chk-'+name, monitor='val_binary_accuracy', mode='max', verbose=1, save_best_only=True)
    keras.callbacks.EarlyStopping(monitor='loss', patience=100),
    keras.callbacks.EarlyStopping(monitor='val_mean_squared_error', patience=100),
]
history = model.fit(train_dataset, epochs=5000, callbacks=callbacks, validation_data=test_dataset)

print()
print("Done")

dmetrics = model.evaluate(train_dataset, verbose=0)
tmetrics = model.evaluate(test_dataset, verbose=0)

print(dmetrics)
print(tmetrics)

name = '%.04f-%.04f-mse%.04f-m%s'%(dmetrics[1],tmetrics[1],tmetrics[2],name)

print()
if dmetrics[1] > 0.96 and tmetrics[1] > 0.96:
	print('Saving as ' + name)
	model.save(name)
else:
	print('NOT saving this crappy result: ' + name)

print()
for (f,d) in test_data.items():
    metrics = model.evaluate(d[0],d[1], verbose=0)
    print('%-30s = %8.04f' % (os.path.basename(f), metrics[1]))

print()
print('Name was ' + name)
