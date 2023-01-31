# -*- mode: ruby -*-
# vi: set ft=ruby :
# frozen_string_literal: true

Vagrant.configure('2') do |config|
  config.vm.box = 'ubuntu/focal64'

  config.vm.hostname = 'mumak-vm'

  config.vm.provider 'virtualbox' do |vb|
    vb.cpus = 4
    vb.memory = '6144'
    vb.customize ['modifyvm', :id, '--uart1', '0x3F8', '4']
    vb.customize ['modifyvm', :id, '--uartmode1', 'file', File::NULL]
  end

  # Install Docker
  config.vm.provision :docker

  config.vm.provision 'shell', path: 'vm-scripts/setup.sh', privileged: true

  config.vm.provision 'shell', path: 'vm-scripts/custom.sh', privileged: false if File.exist?('vm-scripts/custom.sh')

  config.vm.provision 'shell', inline: <<-SHELL
    echo 'source /vagrant/vm-scripts/env.sh'>>/home/vagrant/.bashrc
  SHELL

  config.vm.provision 'shell', :run => 'always', inline: <<-SHELL
    mkdir -p /mnt/pmem0
    mkdir -p /mnt/ramdisk
    mount -t tmpfs -o rw,size=2G tmpfs /mnt/pmem0
    mount -t tmpfs -o rw,size=2G tmpfs /mnt/ramdisk
    chown vagrant /mnt/pmem0/
    chown vagrant /mnt/ramdisk/
  SHELL

  config.vm.provision 'shell', path: 'scripts/setup_host.sh', :run => 'always', privileged: true

  config.ssh.extra_args = ['-t', 'cd /vagrant; tmux a || tmux']
end
