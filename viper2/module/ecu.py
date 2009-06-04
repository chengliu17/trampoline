###############################################################################
# IMPORTS
###############################################################################
import re, sys
import threading, time #ReadingThread

sys.path.append('ipc')
import ipc
from device import Device

###############################################################################
# READING THREAD CLASS
###############################################################################
class ReadingThread(threading.Thread):
  """
  Reading thread block on fifo access while fifo is empty.
  When fifo is not empty, reading thread add an event to the 
  ecu. Next, ecu add an event to the good device one, and scheduler
  wake up the device. The scheduler give modifiedRegisters mask to
  event too.
  """

  def __init__(self, ipc, ecu):
    """
    Constructor.
    @param ipc ipc structure to access fifo with ipc_mod module
    @ecu ecu to add event.
    """
    threading.Thread.__init__(self)
    self.__ipc = ipc
    self.__ecu = ecu

    self.__run = True

  def run(self):
    """
    The threaded function.
    Wait that fifo is not empty.
    """
    while self.__run:
      """ Get modified device and reg """
      modified = ipc.tpl_ipc_pop_fifo(self.__ipc);

      """ Call Ecu event handler """
      device = ipc.tpl_ipc_reg_to_dev(modified.dev)
      self.__ecu.event(device, modified.reg_mask)

  def kill(self):
    """
    Call to stop the main loop of the reading thread.
    """
    self.__run = False

###############################################################################
# ECU CLASS
###############################################################################
class Ecu(object):
  """
  Ecu class.
  Ecu (Engine control unit) represent devices and an operating system.
  """
  
  def __init__(self, osPath, scheduler, devices = None):
    """
    Constructor.
    @osPath trampoline os executable to use with this Ecu.
    @param devices (optionnal) devices list to use in Ecu.
    """
    """ Copy """
    self.__osPath = osPath
    self.__scheduler = scheduler
    self.__dir = re.match(r'.*/+', osPath).group() #get dir from osPath
    
    """ Init """
    self.__devices       = {} # Dict
    self.__offset        = 32 # 32 last bits are used by registers TODO Get it in consts.h
    self.__ipc           = None
    self.__readingThread = None # No reading thread if we only generate

    """ Add devices """
    if devices != None:
      self.add(devices)

  def add(self, devices):
    """
    Add devices to the ecu.
    @param devices the devices list used by the Ecu (class Device)
    """
    for device in devices:
      self.__devices[device.id] = device

  def start(self):
    """
    Run trampoline process, run reading thread and devices.
    """
    
    """ IPC """
    self.__ipc = ipc.tpl_ipc_create_instance(self.__osPath)
    if not self.__ipc:
      raise ValueError, "You must compile trampoline before run viper 2"  
      # TODO Create IPCError class

    """ Init devices """ 
    for name, device in self.__devices.iteritems():
      device.setEcu(self)
      device.setScheduler(self.__scheduler)
      device.start()

    """ Init and start reading thread """
    self.__readingThread = ReadingThread(self.__ipc, self)
    self.__readingThread.setDaemon(True) # Can stop script even if thread is running
    self.__readingThread.start()

  def generate(self):
    """
    Generate the header file use to compile trampolin with the same identifiers
    than viper 2
    """

    """ Open header """
    try:
      header = open(self.__dir + "vp_ipc_devices.h", "w")
      oilFile = open(self.__dir + "target.cfg", "w")
    except IOError:
      print "Can't access to " + self.__dir + "vp_ipc_devices.h"
      print " or " + self.__dir + "target.cfg"
      raise IOError, "You should verify dir \""+ self.__dir + "\" exists and it can be writable"

    """ Generate header """
    header.write("#ifndef __VP_DEVICES_H__\n#define __VP_DEVICES_H__\n")
    header.write('\n#include "ipc/com.h" /* reg_id_t, dev_id_t */\n')
    oilFile.write("interrupts{\n")

    """ Generate device identifier """
    index = 0
    header.write("\n/* Devices */\n")
    for name, device in self.__devices.iteritems(): 
      index += 1
      header.write("const reg_id_t " + device.name + " = " + hex(device.id << self.__offset) + ";\n")
      oilFile.write("  " + device.name + " = " + str(device.callbackIndex) + ";\n")

    """ Generate register identifier """
    header.write("\n/* Registers */\n")
    for name, device in self.__devices.iteritems():
      device.generateRegisters(header)

    """ Generate matchless registers identifiers """
    header.write("\n/* Completes registers */\n")
    for name, device in self.__devices.iteritems():
      device.generate(header)

    """ Generate footer """
    header.write("\n#endif /* __VP_DEVICES_H__ */\n")
    oilFile.write("};\n")

    header.close()
    oilFile.close()

  def sendIt(self, signum, id):
    """
    Send an UNIX interruption (kill).
    @pararm signum must be Device.SIGUSR1, Device.SIGUSR2 or Device.SIGALRM 
    @param id the device id (tiny one [without '<<'])
    """
    ipc.tpl_ipc_send_it(self.__ipc, signum, id)

  def kill(self):
    """ Stop reading thread """
    if self.__readingThread:
      self.__readingThread.kill()
      if self.__readingThread.isAlive():
        print "Waiting reading thread 1 secondes..."
        time.sleep(1)
        if self.__readingThread.isAlive():
          print "Reading thread stop not clearly."

    """ Stop IPC """
    if self.__ipc:
      ipc.tpl_ipc_destroy_instance(self.__ipc)

  def writeRegister(self, registerID, info):
    """
    Write an information into a register of the shared memory
    @param registerID a full register id (device|reg)
    @param info an 32 bits integer
    """
    ipc.tpl_ipc_write_reg(self.__ipc, registerID, info)

  def readRegister(self, registerID):
    """
    Read an information from a register of the shared memory
    @param registerID a full register id (device|reg)
    @return the information (32 bits integer)
    """
    return ipc.tpl_ipc_read_reg(self.__ipc, registerID) 

  def event(self, deviceID, registersMask):
    """
    Add an event to the scheduler list and wakes up it.
    @param deviceID device id
    @param registerMask register mask (reg_id_t)
    """
    if deviceID not in self.__devices:
      raise ValueError, str(deviceID) + " is not in devices list !" # TODO IPCError ?
    else:
      self.__scheduler.addEvent(
          Event(self.__devices[deviceID], 0, registersMask)
      )