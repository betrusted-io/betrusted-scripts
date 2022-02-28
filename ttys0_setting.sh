#sudo stty -F /dev/ttyS0 -ixon

# this should work better than the above
sudo screen -fn /dev/ttyS0 115200
