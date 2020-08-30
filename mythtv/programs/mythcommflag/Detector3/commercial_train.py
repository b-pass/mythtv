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
        parts[4] = (sec % 3600) / 15.0 / (3600.0/15.0)
        
        #f = parts[2].split('-', 2)
        #flen = (int(f[1].strip()) - int(f[0].strip()))
        #lp = max(0.0,min(float(parts[6]),1.0))
        #logo += lp * flen
        #nologo += (1.0 - lp) * flen
        
        d = [max(0.0,min(float(x),1.0)) for x in parts[3:]]
        d.append(last_ans)
        #d.append(min(ctime/1200.0, 1.0))
        data.append(d)
        
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

#import tensorflow as tf
import tensorflow.compat.v1 as tf
tf.disable_v2_behavior()
import numpy as np

#tf.set_random_seed(int(datetime.datetime.now().timestamp()))
tf.set_random_seed(111712)
#random.seed(121117)

num_data_cols = len(next (iter (input_data.values()))[0][0])

learning_rate = 0.1
dropout = 1.0

inputs = tf.placeholder(tf.float64, [None, num_data_cols], name="inputs") 
answers = tf.placeholder(tf.float64, [None, 1], name="answers") 
keep_prob = tf.placeholder(tf.float64, name="keep_prob")

network = inputs
dim = num_data_cols
h = dim #int((num_data_cols + 1) / 2.0 + 1)

with tf.name_scope('input'):
    weights_h = tf.Variable(tf.random_normal(mean=0.5, stddev=0.25, dtype=tf.float64, shape=[dim, h]), name='weights')
    biases_h = tf.Variable(tf.random_normal(mean=0.5, stddev=0.25, dtype=tf.float64, shape=[h]), name='biases')
    tf.summary.histogram("weights", weights_h)
    tf.summary.histogram("biases", biases_h)
    network = tf.matmul(network, weights_h) + biases_h
name = str(h)

layers = 2
h = int((h + 1) / layers)
for i in range(layers):
    with tf.name_scope('layer' + str(i)):
        name += '-' + str(h)
        weights_h = tf.Variable(tf.random_normal(mean=0.5, stddev=0.25, dtype=tf.float64, shape=[dim, h]), name='weights')
        biases_h = tf.Variable(tf.random_normal(mean=0.5, stddev=0.25, dtype=tf.float64, shape=[h]), name='biases')
        network = tf.nn.tanh(tf.matmul(network, weights_h) + biases_h)
        if dropout < 1.0:
            network = tf.nn.dropout(network, keep_prob)
        tf.summary.histogram("weights", weights_h)
        tf.summary.histogram("biases", biases_h)
        
    dim = h

name += '-1'
if dropout < 1.0:
    name += '_d' + str(dropout)

with tf.name_scope('output'):
    weights_out = tf.Variable(tf.random_normal(mean=0.5, stddev=0.25, dtype=tf.float64, shape=[dim, 1]), name='weights')
    tf.summary.histogram("weights", weights_out)
    network = tf.matmul(network, weights_out)

loss = tf.reduce_mean(tf.nn.sigmoid_cross_entropy_with_logits(labels=answers, logits=network), name="loss")
tf.summary.scalar("loss", loss)

network = tf.nn.sigmoid(network, name="main")

#correct_prediction = tf.equal(tf.argmax(network, 1), tf.argmax(answers, 1))
correct_prediction = tf.equal(tf.less_equal(network, 0.5), tf.less_equal(answers, 0.5), name="correct_prediction")
accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float64), name="accuracy")
tf.summary.scalar("accuracy_scalar", accuracy)

train_step = tf.train.AdamOptimizer(learning_rate).minimize(loss)

with tf.control_dependencies([train_step]):
    loss_after_optimizer = tf.identity(loss)
    accuracy_after_optimizer = tf.identity(accuracy)

#name = str(layers) + 'x' + str(h) + '_d' + str(dropout)
print("Name base = " + name)

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

#all_everything = list(zip(all_input_data, all_input_answers))
#all_input_data, all_input_answers = zip(*all_everything)

sess = tf.InteractiveSession()
tf.global_variables_initializer().run()

#print(sess.run(accuracy, feed_dict={inputs: input_data, keep_prob: 1.0, answers: input_answers}))
saver = tf.train.Saver()
best_self = 0
best_other = 0
merged_summary_op = tf.summary.merge_all();
writer = tf.summary.FileWriter('/tmp/tensorboard', sess.graph)

for x in range(1000000):
    if tdx:
        all_input_data += all_test_data
        all_input_answers += all_test_answers
        z = list(zip(all_input_data, all_input_answers))
        random.shuffle(z)
        p = int(len(z) * float(tdx)/100.0)
        a = z[0:p]
        b = z = z[p:]
        all_test_data, all_test_answers = zip(*a)
        all_input_data, all_input_answers = zip(*b)
        tdx = 0
    try:
        #for (x,y) in all_everything:
        #    sess.run([train_step, loss, accuracy], feed_dict={inputs: [x], keep_prob: dropout, answers: [y]})
        for y in range(100):
            train_result = sess.run([train_step, loss, accuracy], feed_dict={inputs: all_input_data, keep_prob: dropout, answers: all_input_answers})
            if train_result[2] > best_self:
                break
        if dropout < 1.0:
            train_result = sess.run([train_step, loss, accuracy], feed_dict={inputs: all_input_data, keep_prob: 1.0, answers: all_input_answers})
        self_check = train_result[2]
        self_loss = train_result[1]
        
        for (f, d) in input_data.items():
            #for ii in range(len(d[0])):
            #    a = sess.run(audio, feed_dict={inputs: d[0][ii:ii+1], keep_prob:1.0})
            #    print(str(ii) + ") a=" + str(a))
            #for ii in range(len(d[0])):
            #    c = sess.run(network, feed_dict={inputs: d[0][ii:ii+3], keep_prob:1.0})
            #    a = sess.run(accuracy, feed_dict={inputs: d[0][ii:ii+3], answers:d[1][ii:ii+3], keep_prob:1.0})
            #    p = sess.run(correct_prediction, feed_dict={inputs: d[0][ii:ii+3], answers:d[1][ii:ii+3], keep_prob:1.0})
            #    print(str(ii) + ") ans=" + str(d[1][ii:ii+3]) + " est=" + str(c) + " acc=" + str(a) + ", corr=" + str(p))
            res = sess.run(accuracy, feed_dict={inputs: d[0], keep_prob: 1.0, answers: d[1]})
            print(f + " = " + str(res))
        
        test_check = 0
        if all_test_data:
            for (f, d) in test_data.items():
                #for i in range(len(d[0])):
                #    res = sess.run(network, feed_dict={inputs: [d[0][i]], keep_prob: 1.0, answers: [d[1][i]]})
                #    if (res[0][0] <= 0.5) != (d[1][i][0] <= 0.5):
                #        print(f + '[' + str(i) + '] = ' + str(d[1][i]) + ' ? ' + str(res[0]))
                res = sess.run(accuracy, feed_dict={inputs: d[0], keep_prob: 1.0, answers: d[1]})
                print(f + " = " + str(res))
            test_check = sess.run(accuracy, feed_dict={inputs: all_test_data, keep_prob: 1.0, answers: all_test_answers})
            print("Accuracy:", self_check)
            print("Test Acc:", test_check)
        else:
            print("Accuracy:", self_check)
        print("Loss    :", self_loss)
        
        if self_check > best_self or test_check > best_other :#or (self_check+test_check) > (best_self+best_other):
            if self_check >= 0.95 and (test_check > 0.92 or not test_data):
                score = str(self_check) + '_' + str(test_check)
                fn = saver.save(sess, './' + score + '_' + name)
                print("Saved new best", fn)
            else:
                print("New best "+str(self_check)+" | "+str(test_check))
                
        best_self = max(best_self,self_check)
        best_other = max(best_other,test_check)
        
        if best_self >= 1.0:
            break
        writer.add_summary(sess.run(merged_summary_op, feed_dict={inputs: all_input_data, keep_prob: dropout, answers: all_input_answers}), x)
        
    except KeyboardInterrupt:
        break
print("Stopped")
writer.close()
