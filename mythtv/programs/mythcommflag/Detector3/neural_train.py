#!/usr/bin/env python3
from __future__ import print_function
import sys
import re
import random
import datetime

if len(sys.argv) < 2:
    print(sys.argv[0] + " <training_file> [testing_file]")
    print("File format:")
    print('''
Truthiness Start Frame-Frame Len Pos Scenes Logo Formats Sides Center Rear

Truthiness = is this a commercial (for training)
Start = Offset in seconds from start of show (not used)
Frame-Frame = Range of frame numbers for this segment, used in output
Len = Length in seconds of segment
Pos = Normalized position (-1 = first 10 mins, 0 = middle, 1 = last 10 mins)
Scenes = Scene change rate (seconds per scene)
Logo = Logo detection (as percent of frames)
Formats = Number of format changes
Sides = Average peak audio of left and right channels 
Center = Average peak audio of center channel
Read = Average peak audio of rear/surround channels

Truthiness is a raw score, where < 0 is a commercial and > 0 
is a show segment and == 0 is unknown

Audio should be absolute value peak (from 0 to 32768).  If a channel is 
not present, set to 0.  If no audio available, set all to zero.

# comment lines and blank lines are legal and ignored
    ''')
    sys.exit(1)

input_info = []
input_data = []
input_answers = []

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
        elif score < 0:
            answers += [[1.0, 0.0]]
        else:
            answers += [[0.0, 1.0]]
            
        info += [parts[2]]
        
        data += [[float(x) for x in parts[3:]]]
        
    return (info, data, answers)

(input_info, input_data, input_answers) = load(sys.argv[1])
test_info = test_data = test_answers = []
if len(sys.argv) > 2:
    (test_info, test_data, test_answers) = load(sys.argv[2])

import tensorflow as tf
import numpy as np

#tf.set_random_seed(int(datetime.datetime.now().timestamp()))
tf.set_random_seed(1711)

learning_rate = 0.001
num_data_cols = 8
dropout = 0.65
hidden_layers = [128]

inputs = tf.placeholder(tf.float32, [None, num_data_cols], name="inputs") 
answers = tf.placeholder(tf.float32, [None, 2], name="answers") 
keep_prob = tf.placeholder(tf.float32, name="keep_prob")

network = inputs
#network = tf.nn.dropout(network, keep_prob)
dim = num_data_cols
for h in hidden_layers:
    weights_h = tf.Variable(tf.random_uniform([dim, h]))
    biases_h = tf.Variable(tf.random_uniform([h]))
    network = tf.nn.relu(tf.matmul(network, weights_h) + biases_h) # tf.nn.tanh? tf.nn.elu? or both?
    network = tf.nn.dropout(network, keep_prob)
    dim = h

weights_out = tf.Variable(tf.random_uniform([dim, 2]))
biases_out = tf.Variable(tf.random_uniform([2]))
network = tf.matmul(network, weights_out) + biases_out

cross_entropy = tf.reduce_mean(tf.nn.softmax_cross_entropy_with_logits(labels=answers, logits=network), name="cross_entropy")
train_step = tf.train.AdamOptimizer(learning_rate).minimize(cross_entropy)
network = tf.nn.softmax(network, name="main")

sess = tf.InteractiveSession()
tf.global_variables_initializer().run()

correct_prediction = tf.equal(tf.argmax(network, 1), tf.argmax(answers, 1))
accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32), name="accuracy")

name = ''
for h in hidden_layers:
    name += str(h)
    name += 'n_'
name += 'd'
name += str(dropout)

#print(sess.run(accuracy, feed_dict={inputs: input_data, keep_prob: 1.0, answers: input_answers}))
saver = tf.train.Saver()
best_self = 0.5
best_other = 0.5
for x in range(1000):
    try:
        for y in range(10):
            for z in range(100):
                sess.run(train_step, feed_dict={inputs: input_data, keep_prob: dropout, answers: input_answers})
            self_check = sess.run(accuracy, feed_dict={inputs: input_data, keep_prob: 1.0, answers: input_answers})
            #print("Check: "+str(self_check))
            if self_check > best_self:
                best_self = self_check
                if test_data:
                    test_check = sess.run(accuracy, feed_dict={inputs: test_data, keep_prob: 1.0, answers: test_answers})
                else:
                    test_check = 1.0
                #if test_check >= best_other:
                best_other = test_check
                if 1 or best_self > 0.95 and best_other > 0.95:
                    score = str(best_self) + '_' + str(best_other)
                    print("New best "+str(best_self)+" | "+str(best_other) + " (saved)")
                    print(saver.save(sess, score + '_' + name))
                else:
                    print("New best "+str(best_self)+" | "+str(best_other))
    except KeyboardInterrupt:
        break
    continue
    #print(network.eval(feed_dict={inputs: check_input, keep_prob: 1.0, answers: check_answers}))
    self_check = sess.run(accuracy, feed_dict={inputs: input_data, keep_prob: 1.0, answers: input_answers})
    if test_data and test_answers:
        i = 0
        for val in network.eval(feed_dict={inputs: test_data, keep_prob: 1.0, answers: test_answers}):
            print(test_info[i] + " " + str(round(val[1], 4)))
            i += 1
        test_check = sess.run(accuracy, feed_dict={inputs: test_data, keep_prob: 1.0, answers: test_answers})
        print("Check: "+str(self_check)+", Test: "+str(test_check))
    else:
        print("Check: "+str(self_check))
    
