*** These files are used to automatically upload the programs on reboot ***


seve my_daemons.service at /etc/systemd/system/ 

in the terminal:

sudo systemctl daemon-reload
sudo systemctl enable my_daemons.service.service
