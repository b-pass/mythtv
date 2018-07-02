#!/usr/bin/env python3
from __future__ import print_function
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
Truthiness Start Frame-Frame Len Pos Scenes Logo Formats Sides Center Rear

Truthiness = is this a commercial (for training)
Start = Offset in seconds from start of show (not used)
Frame-Frame = Range of frame numbers for this segment, used in output
Len = Length in seconds of segment
Pos = Normalized position indicator
Scenes = Scene change rate (seconds per scene)
Logo = Logo detection (as percent of frames)
Formats = Number of format changes
Sides = Average peak audio of left and right channels 
Center = Average peak audio of center channel
Rear = Average peak audio of rear/surround channels

Truthiness is a raw score, where < 0 is a commercial and >= 0 
is a show segment

Audio should be absolute value peak (from 0 to 32768).  If a channel is 
not present, set to 0.  If no audio available, set all to zero.

# comment lines and blank lines are legal and ignored
    ''')
    sys.exit(1)

def load(filename):
    info = []
    data = []
    answers = []
    for line in open(filename, 'r'):
        line = line.strip()
        if not line or line[0] == '#':
            continue
        
        parts = line.split()
        if len(parts) < 11:
            print("Ignored bad line: " + line)
            continue
        
        if parts[2][-1] == '-':
            parts[2] += parts[3]
            del parts[3]
        
        score = int(parts[0])
        if score == 0:
            continue
        #elif score < 0:
        #    answers += [[1.0, 0.0]]
        #else:
        #    answers += [[0.0, 1.0]]
        answers += [1 if score < 0 else 0]
        
        info += [parts[2]]
        
        data += [[max(0.0,min(float(x),1.0)) for x in parts[3:]]]
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
if "-t" in sys.argv:
    i = len(sys.argv) - 1
    while i > 0:
        f = sys.argv[i]
        i -= 1
        if f == "-t":
            break
        if not os.path.isfile(f):
            print("Not a file",f)
            continue
        if f not in input_data:
            (ix, d, a) = load(f)
            input_data[f] = (d, a)
        test_data[f] = input_data[f]
        del input_data[f]

os.environ['TF_CPP_MIN_LOG_LEVEL'] = '1'

import numpy as np
import tensorflow as tf
from tensorflow.contrib.tensor_forest.python import tensor_forest
from tensorflow.python.ops import resources



#tf.set_random_seed(int(datetime.datetime.now().timestamp()))
tf.set_random_seed(111712)

num_data_cols = len(next (iter (input_data.values()))[0][0])
inputs = tf.placeholder(tf.float32, [None, num_data_cols], name="inputs") 
answers = tf.placeholder(tf.int32, [None], name="answers") 
tf.placeholder(tf.float32, name="keep_prob") # backward compatability

hparams = tensor_forest.ForestHParams(num_classes=2, num_features=num_data_cols).fill()

# Build the Random Forest
forest_graph = tensor_forest.RandomForestGraphs(hparams)

# Get training graph and loss
train_step = forest_graph.training_graph(inputs, answers)

# Measure the accuracy
network, _, _ = forest_graph.inference_graph(inputs)
network = tf.argmax(network, 1, name="main")
correct_prediction = tf.equal(network, tf.cast(answers, tf.int64), name="correct_prediction")
accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float64), name="accuracy")

#with tf.control_dependencies([train_step]):
#    loss_after_optimizer = tf.identity(loss)
#    accuracy_after_optimizer = tf.identity(accuracy)

name = 'forest'

all_input_data = []
all_input_answers = []
all_test_data = []
all_test_answers = []
for (f, d) in input_data.items():
    all_input_data += d[0]
    all_input_answers += d[1]
for (f, d) in test_data.items():
    all_test_data += d[0]
    all_test_answers += d[1]

# Initialize the variables (i.e. assign their default value) and forest resources
init_vars = tf.group(tf.global_variables_initializer(),
						resources.initialize_resources(resources.shared_resources()))

sess = tf.InteractiveSession()

sess.run(init_vars)

saver = tf.train.Saver()
best_self = 0
best_other = 0
for x in range(1000000):
    try:
        z = list(zip(all_input_data, all_input_answers))
        random.shuffle(z)
        all_input_data, all_input_answers = zip(*z)
        for y in range(100):
            _,self_check = sess.run([train_step, accuracy], feed_dict={inputs: all_input_data, answers: all_input_answers})
            if self_check > best_self:
                break
        
        for (f, d) in input_data.items():
            #for ii in range(len(d[0])):
            #    c = sess.run(network, feed_dict={inputs: d[0][ii:ii+3]})
            #    a = sess.run(accuracy, feed_dict={inputs: d[0][ii:ii+3], answers:d[1][ii:ii+3]})
            #    p = sess.run(correct_prediction, feed_dict={inputs: d[0][ii:ii+3], answers:d[1][ii:ii+3]})
            #    print(str(ii) + ") ans=" + str(d[1][ii:ii+3]) + " est=" + str(c) + " acc=" + str(a) + ", corr=" + str(p))
            res = sess.run(accuracy, feed_dict={inputs: d[0], answers: d[1]})
            print(f + " = " + str(res))
        
        test_check = 0
        if test_data:
            for (f, d) in test_data.items():
                res = sess.run(accuracy, feed_dict={inputs: d[0], answers: d[1]})
                print(f + " = " + str(res))
            test_check = sess.run(accuracy, feed_dict={inputs: all_test_data, answers: all_test_answers})
            print("Accuracy:", self_check)
            print("Test Acc:", test_check)
        else:
            print("Accuracy:", self_check)
        #print("Loss    :", self_loss)
        
        if self_check > best_self:# or test_check > best_other:
            best_self = max(best_self,self_check)
            best_other = max(best_other,test_check)
            if self_check >= 0.98:# and (test_check > 0.95 or not test_data):
                score = str(self_check) + '_' + str(test_check)
                fn = saver.save(sess, './' + score + '_' + name)
                print("Saved new best", fn)
            else:
                print("New best "+str(self_check)+" | "+str(test_check))
        if self_check >= 1.0:
            break
    except KeyboardInterrupt:
        break
print("Stopped")
