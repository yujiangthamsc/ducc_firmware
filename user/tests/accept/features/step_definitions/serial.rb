require 'concurrent/synchronization'
require 'singleton'

module Particle
  class SerialPort
    def initialize(serial, name)
      @serial = serial
      @name = name
      @lock = Concurrent::Synchronization::Lock.new
      @data = ''

      @serial.on_data do |data|
        @lock.synchronize do
          @data.concat(data)
          @lock.broadcast
        end
      end
    end

    def write(data)
      @serial.send_data(data)
    end

    def close
      @serial.close_connection
    end

    def check_data(timeout_sec, condition)
      last_except = nil
      # Keep calling provided block until it returns 'true' for current data
      ok = @lock.wait_until(timeout_sec) do
        begin
          if @data.ascii_only?
              d = @data.gsub("\r\n", "\n") # CRLF -> LF
              d.rstrip! # Strip trailing newline characters
          else
              d = @data
          end
          condition.call(d)
        rescue RSpec::Expectations::ExpectationNotMetError => e
          last_except = e
          false
        end
      end
      if !ok
        if last_except
          raise last_except # Rethrow last exception
        else
          raise "#{@name} output doesn't match expected data" # Generic error message
        end
      end
    end
  end

  class Serial
    include Singleton

    attr_reader :ports

    def initialize
      @ports = {
        'Serial' => Util.env_var('PARTICLE_SERIAL_DEV'),
        'Serial1' => Util.env_var('PARTICLE_SERIAL1_DEV'),
        'USBSerial1' => Util.env_var('PARTICLE_USB_SERIAL1_DEV')
      }

      @active_ports = {}
    end

    def open(port, baud, data_bits = 8)
      raise "Serial port is already open: #{port}" if @active_ports.has_key?(port)
      dev = @ports[port]
      raise "Unknown serial port: #{port}" unless dev
      serial = EventMachine.open_serial(dev, baud, data_bits)
      @active_ports[port] = SerialPort.new(serial, port)
    end

    def close(port)
      active_port(port).close
      @active_ports.delete(port)
    end

    def close_all
      @active_ports.each_value do |p|
        p.close
      end
      @active_ports.clear
    end

    def write(port, data)
      d = if data.ascii_only?
          data.gsub("\n", "\r\n") # LF -> CRLF
        else
          data
        end
      active_port(port).write(d)
    end

    def check_data(port, timeout_sec = 3, &condition)
      active_port(port).check_data(timeout_sec, condition)
    end

    alias_method :reset, :close_all

    private

      def active_port(port)
        p = @active_ports[port]
        raise "Serial port is not open: #{port}" unless p
        p
      end
  end
end

serial = Particle::Serial.instance

# Step definitions
Given(/^(.*Serial.*) is open(?: with baud rate set to (\d+))?$/) do |port, baud|
  baud ||= 9600
  serial.open(port, baud)
end

When(/^I write to (.*Serial.*)$/) do |port, data|
  serial.write(port, data)
end

Then(/^(.*Serial.*) output should (be|contain|match|start with|end with)$/) do |port, op, data|
  serial.check_data(port) do |d|
    Particle::Util.should(d, data, op)
  end
end
