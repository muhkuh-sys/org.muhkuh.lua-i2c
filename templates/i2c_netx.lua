local class = require 'pl.class'
local I2CNetx = class()


function I2CNetx:_init(tLog)
  self.I2C_CMD_Open = ${I2C_CMD_Open}
  self.I2C_CMD_RunSequence = ${I2C_CMD_RunSequence}
  self.I2C_CMD_Close = ${I2C_CMD_Close}

  self.I2C_SEQ_COMMAND_Read = ${I2C_SEQ_COMMAND_Read}
  self.I2C_SEQ_COMMAND_Write = ${I2C_SEQ_COMMAND_Write}
  self.I2C_SEQ_COMMAND_Delay = ${I2C_SEQ_COMMAND_Delay}

  self.I2C_SEQ_CONDITION_None = ${I2C_SEQ_CONDITION_None}
  self.I2C_SEQ_CONDITION_Start = ${I2C_SEQ_CONDITION_Start}
  self.I2C_SEQ_CONDITION_Stop = ${I2C_SEQ_CONDITION_Stop}
  self.I2C_SEQ_CONDITION_Continue = ${I2C_SEQ_CONDITION_Continue}

  self.I2C_SETUP_CORE_RAPI2C0 = ${I2C_SETUP_CORE_RAPI2C0}
  self.I2C_SETUP_CORE_RAPI2C1 = ${I2C_SETUP_CORE_RAPI2C1}
  self.I2C_SETUP_CORE_RAPI2C2 = ${I2C_SETUP_CORE_RAPI2C2}
  self.I2C_SETUP_CORE_RAPI2C3 = ${I2C_SETUP_CORE_RAPI2C3}
  self.I2C_SETUP_CORE_RAPI2C4 = ${I2C_SETUP_CORE_RAPI2C4}
  self.I2C_SETUP_CORE_RAPI2C5 = ${I2C_SETUP_CORE_RAPI2C5}
  self.I2C_SETUP_CORE_I2C0 = ${I2C_SETUP_CORE_I2C0}
  self.I2C_SETUP_CORE_I2C1 = ${I2C_SETUP_CORE_I2C1}
  self.I2C_SETUP_CORE_I2C2 = ${I2C_SETUP_CORE_I2C2}

  self.I2C_HANDLE_SIZE = ${SIZEOF_I2C_HANDLE_STRUCT}

  self.romloader = require 'romloader'
  self.lpeg = require 'lpeglabel'
  self.pl = require'pl.import_into'()

  self.tLog = tLog

  self.tGrammarI2cMacro = self:__create_i2c_macro_grammar()

  self.ucDefaultRetries = 16
end



function I2CNetx:__create_i2c_macro_grammar()
  local lpeg = self.lpeg

  local Space = lpeg.V('Space')
  local SinglequotedString = lpeg.V('SinglequotedString')
  local DoublequotedString = lpeg.V('DoublequotedString')
  local QuotedString = lpeg.V('QuotedString')
  local DecimalInteger = lpeg.V('DecimalInteger')
  local HexInteger = lpeg.V('HexInteger')
  local BinInteger = lpeg.V('BinInteger')
  local Integer = lpeg.V('Integer')
  local Data = lpeg.V('Data')
  local StartCommand = lpeg.V('StartCommand')
  local StopCommand = lpeg.V('StopCommand')
  local ReadCommand = lpeg.V('ReadCommand')
  local WriteCommand = lpeg.V('WriteCommand')
  local DelayCommand = lpeg.V('DelayCommand')
  local Command = lpeg.V('Command')
  local Comment = lpeg.V('Comment')
  local Statement = lpeg.V('Statement')
  local Program = lpeg.V('Program')

  return lpeg.P{ Program,
    -- An program is a list of statements separated by newlines.
    Program = Space * lpeg.Ct(lpeg.S("\r\n")^0 * Statement * (lpeg.S("\r\n")^1 * Statement)^0 * lpeg.S("\r\n")^0) * -1;

    -- A statement is either a command or a comment.
    Statement = Space * (Command + Comment) * Space;

    -- A comment starts with a hash and covers the complete line.
    Comment = lpeg.P('#') * (1 - lpeg.S("\r\n"))^0;

    -- A command is one of the 5 possible commands.
    Command = lpeg.Ct(Space * (StartCommand + StopCommand + ReadCommand + WriteCommand + DelayCommand) * Comment^-1 * Space);

    -- A start command has no parameter.
    StartCommand = lpeg.Cg(lpeg.P("start"), 'cmd');

    -- A stop command has no parameter.
    StopCommand = lpeg.Cg(lpeg.P("stop"), 'cmd');

    -- A read command has the address, a length parameter and an optional retry.
    ReadCommand = lpeg.Cg(lpeg.P("read"), 'cmd') * Space * lpeg.Cg(Integer, 'address') * Space * lpeg.P(',') * Space * lpeg.Cg(Integer, 'length') * (Space * lpeg.P(',') * Space * lpeg.Cg(Integer, 'retries'))^-1;

    -- A write command has the address, an optional retry and a data definition as parameters.
    WriteCommand = lpeg.Cg(lpeg.P("write"), 'cmd') * Space * lpeg.Cg(Integer, 'address') * Space * lpeg.P(',') * Space * Data * (Space * lpeg.P(',') * Space * lpeg.Cg(Integer, 'retries'))^-1;

    -- A delay command has the delay in milliseconds as the parameter.
    DelayCommand = lpeg.Cg(lpeg.P("delay"), 'cmd') * Space * lpeg.Cg(Integer, 'delay'); 

    -- A data definition is a list of comma separated integers or strings surrounded by curly brackets. 
    Data = lpeg.Ct(lpeg.P('{') * Space * (lpeg.Cg(QuotedString) + lpeg.Cg(Integer)) * Space * (lpeg.P(',') * Space * (lpeg.Cg(QuotedString) + lpeg.Cg(Integer)))^0 * Space * lpeg.P('}'));

    -- A string can be either quoted or double quoted.
    SinglequotedString = lpeg.P("'") * ((1 - lpeg.S("'\r\n\f\\")) + (lpeg.P('\\') * 1))^0 * "'";
    DoublequotedString = lpeg.P('"') * ((1 - lpeg.S('"\r\n\f\\')) + (lpeg.P('\\') * 1))^0 * '"';
    QuotedString = lpeg.P(SinglequotedString + DoublequotedString);

    -- An integer can be decimal, hexadecimal or binary.
    DecimalInteger = lpeg.R('09')^1;
    HexInteger = lpeg.P("0x") * lpeg.P(lpeg.R('09') + lpeg.R('af'))^1;
    BinInteger = lpeg.P("0b") * lpeg.R('01')^1;
    Integer = lpeg.P(HexInteger + BinInteger + DecimalInteger);

    -- Whitespace.
    Space = lpeg.S(" \t")^0;
  }
end



function I2CNetx:initialize(tPlugin)
  local tLog = self.tLog
  local romloader = self.romloader
  local tester = _G.tester

  local astrBinaryName = {
    [romloader.ROMLOADER_CHIPTYP_NETX4000_RELAXED] = '4000',
    [romloader.ROMLOADER_CHIPTYP_NETX4000_FULL]    = '4000',
    [romloader.ROMLOADER_CHIPTYP_NETX4100_SMALL]   = '4000',
--    [romloader.ROMLOADER_CHIPTYP_NETX500]          = '500',
--    [romloader.ROMLOADER_CHIPTYP_NETX100]          = '500',
--    [romloader.ROMLOADER_CHIPTYP_NETX90_MPW]       = '90_mpw',
--    [romloader.ROMLOADER_CHIPTYP_NETX90]           = '90',
--    [romloader.ROMLOADER_CHIPTYP_NETX56]           = '56',
--    [romloader.ROMLOADER_CHIPTYP_NETX56B]          = '56',
--    [romloader.ROMLOADER_CHIPTYP_NETX50]           = '50',
--    [romloader.ROMLOADER_CHIPTYP_NETX10]           = '10'
  }

  -- Get the binary for the ASIC.
  local tAsicTyp = tPlugin:GetChiptyp()
  local strBinary = astrBinaryName[tAsicTyp]
  if strBinary==nil then
    error('No binary for chip type %s.', tAsicTyp)
  end
  local strNetxBinary = string.format('netx/i2c_netx%s.bin', strBinary)
  tLog.debug('Loading binary "%s"...', strNetxBinary)

  local aAttr = tester:mbin_open(strNetxBinary, tPlugin)
  tester:mbin_debug(aAttr)
  tester:mbin_write(tPlugin, aAttr)

  return {
    plugin = tPlugin,
    attr = aAttr
  }
end



function I2CNetx:__parseNumber(strNumber)
  local tResult
  if string.sub(strNumber, 1, 2)=='0b' then
    tResult = 0
    local uiPos = 0
    for iPos=string.len(strNumber),3,-1 do
      local uiDigit = string.byte(strNumber, iPos) - 0x30
      tResult = tResult + uiDigit * math.pow(2, uiPos)
      uiPos = uiPos + 1
    end
  else
    tResult = tonumber(strNumber)
  end

  return tResult
end



function I2CNetx:__combineConditions(atConditions)
  -- Combine the conditions.
  local ucConditions = 0
  local atConditionIdToValue = {
    none = self.I2C_SEQ_CONDITION_None,
    start = self.I2C_SEQ_CONDITION_Start,
    stop = self.I2C_SEQ_CONDITION_Stop,
    continue = self.I2C_SEQ_CONDITION_Continue
  }
  for strCondition in pairs(atConditions) do
    local ucCondition = atConditionIdToValue[strCondition]
    if ucCondition==nil then
      tLog.error('Unknown condition: "%s".', strCondition)
      error('Unknown condition.')
    end
    ucConditions = ucConditions + ucCondition
  end
  return ucConditions
end



function I2CNetx:__uint16_to_bytes(usData)
  local ucB1 = math.floor(usData/256)
  local ucB0 = usData - 256*ucB1

  return ucB0, ucB1
end



function I2CNetx:__uint32_to_bytes(ulData)
  local ucB3 = math.floor(ulData/0x01000000)
  ulData = ulData - 0x01000000*ucB3
  local ucB2 = math.floor(ulData/0x00010000)
  ulData = ulData - 0x00010000*ucB2
  local ucB1 = math.floor(ulData/0x00000100)
  local ucB0 = ulData - 0x00000100*ucB1

  return ucB0, ucB1, ucB2, ucB3
end



function I2CNetx:parseI2cMacro(strMacro)
  local lpeg = self.lpeg
  local tLog = self.tLog
  local pl = self.pl

  local uiExpectedReadData = 0
  local tResult = lpeg.match(self.tGrammarI2cMacro, strMacro)
  if tResult==nil then
    error('Failed to parse the macro...')
  else
--    pl.pretty.dump(tResult)

    -- This is the last command.
    local tCommandStack = nil

    -- Collect the merged commands here.
    local atCmdMerged = {}

    for uiCommandCnt, tRawCommand in ipairs(tResult) do
      local strCmd = tRawCommand.cmd
      if strCmd=='start' then
        -- A start command can not follow another start command.
        if tCommandStack~=nil then
          if tCommandStack.cmd=='start' then
            tLog.error('A "start" command follows another "start" command. This is not allowed.')
            error('Invalid position of start command.')
          end
        end
        -- Replace the command stack with the start command.
        tCommandStack = { cmd='start' }
      elseif strCmd=='stop' then
        -- A stop command must follow a read or write command.
        if tCommandStack==nil then
          tLog.error('Found a stop command without a previous read or write command.')
        elseif tCommandStack.cmd=='read' or tCommandStack.cmd=='write' then
          -- Add the stop condition to the command.
          tCommandStack.conditions['stop'] = true
          -- Remove the command from the stack.
          tCommandStack = nil
        end
      elseif strCmd=='read' then
        -- Create a new read command.
        local tCmd = {
          cmd = 'read',
          conditions = {},
          address = self:__parseNumber(tRawCommand.address),
          length = self:__parseNumber(tRawCommand.length)
        }
        -- Was the last command a "start" command?
        if tCommandStack~=nil and tCommandStack.cmd=='start' then
          tCmd.conditions['start'] = true
        end
        -- Add the optional retries.
        local strRetries = tRawCommand.retries or self.ucDefaultRetries
        tCmd.retries = self:__parseNumber(strRetries)

        table.insert(atCmdMerged, tCmd)
        tCommandStack = tCmd
      elseif strCmd=='write' then
        -- Create a new write command.
        local tCmd = {
          cmd = 'write',
          conditions = {},
          address = self:__parseNumber(tRawCommand.address),
          data = nil
        }
        -- Was the last command a "start" command?
        if tCommandStack~=nil and tCommandStack.cmd=='start' then
          tCmd.conditions['start'] = true
        end
        -- Add the optional retries.
        local strRetries = tRawCommand.retries or self.ucDefaultRetries
        tCmd.retries = self:__parseNumber(strRetries)
        -- Collect the data.
        local astrData = {}
        local astrReplace = {
          ['\\"'] = '"',
          ["\\'"] = "'",
          ['\\a'] = '\a',
          ['\\b'] = '\b',
          ['\\f'] = '\f',
          ['\\n'] = '\n',
          ['\\r'] = '\r',
          ['\\t'] = '\t',
          ['\\v'] = '\v'
        }
        for uiDataElement, strData in ipairs(tRawCommand[1]) do
          if string.sub(strData, 1, 1)=='"' or string.sub(strData, 1, 1)=="'" then
            -- Unquote the string.
            strData = string.sub(strData, 2, -2)
            -- Unescape the string.
            strData = string.gsub(strData, '(\\["\'abfnrtv])', astrReplace)
            table.insert(astrData, strData)
          else
            local uiData = self:__parseNumber(strData)
            if uiData<0 or uiData>255 then
              tLog.error('Data element %d of command %d exceeds the 8 bit range: %d.', uiData, uiCommandCnt, tData)
              error('Invalid data.')
            end
            table.insert(astrData, string.char(uiData))
          end
        end
        tCmd.data = table.concat(astrData)

        table.insert(atCmdMerged, tCmd)
        tCommandStack = tCmd
      elseif strCmd=='delay' then
        -- Create a new delay command.
        local tCmd = {
          cmd = 'delay',
          delay = self:__parseNumber(tRawCommand.delay)
        }
        table.insert(atCmdMerged, tCmd)
        tCommandStack = tCmd
      end
    end

--    pl.pretty.dump(atCmdMerged)

    -- Loop over all merged commands and generate a binary stream.
    local astrMacro = {}
    for _, tCmd in ipairs(atCmdMerged) do
      if tCmd.cmd=='read' then
        local usLen = tCmd.length
        uiExpectedReadData = uiExpectedReadData + usLen

        local ucLen0, ucLen1 = self:__uint16_to_bytes(usLen)
        table.insert(astrMacro, string.char(
          self.I2C_SEQ_COMMAND_Read,
          self:__combineConditions(tCmd.conditions),
          tCmd.address,
          tCmd.retries,
          ucLen0, ucLen1
        ))

      elseif tCmd.cmd=='write' then
        local ucLen0, ucLen1 = self:__uint16_to_bytes(string.len(tCmd.data))
        table.insert(astrMacro, string.char(
          self.I2C_SEQ_COMMAND_Write,
          self:__combineConditions(tCmd.conditions),
          tCmd.address,
          tCmd.retries,
          ucLen0, ucLen1
        ))
        table.insert(astrMacro, tCmd.data)

      elseif tCmd.cmd=='delay' then
        local ucDelay0, ucDelay1, ucDelay2, ucDelay3 = self:__uint32_to_bytes(tCmd.delay)
        table.insert(astrMacro, string.char(
          self.I2C_SEQ_COMMAND_Delay,
          ucDelay0, ucDelay1, ucDelay2, ucDelay3
        ))

      else
        tLog.error('Unknown command: "%s".', tCmd.cmd)
        error('Unknown command.')
      end
    end

    tResult = table.concat(astrMacro)
  end

  return tResult, uiExpectedReadData
end



function I2CNetx:openDevice(tHandle, tCoreID, ucMMIO_SCL, ucMMIO_SDA, usPortcontrol_SCL, usPortcontrol_SDA)
  ucMMIO_SCL = ucMMIO_SCL or 0xff
  ucMMIO_SDA = ucMMIO_SDA or 0xff
  usPortcontrol_SCL = usPortcontrol_SCL or 0xffff
  usPortcontrol_SDA = usPortcontrol_SDA or 0xffff
  local tLog = self.tLog
  local tester = _G.tester
  local aAttr = tHandle.attr

  -- Setup a basic layout of the buffer:
  --   * Parameter (fixed size: 64 bytes)
  --   * Handle (fixed size: I2C_HANDLE_SIZE bytes)
  --   * RX/TX buffer
  tHandle.ulHandleAddress = aAttr.ulParameterStartAddress + 64
  tHandle.ulBufferAddress = aAttr.ulParameterStartAddress + 64 + self.I2C_HANDLE_SIZE

  -- Combine all options.
  local ucCore0, ucCore1 = self:__uint16_to_bytes(tCoreID)
  local ucPSCL0, ucPSCL1 = self:__uint16_to_bytes(usPortcontrol_SCL)
  local ucPSDA0, ucPSDA1 = self:__uint16_to_bytes(usPortcontrol_SDA)
  local strOptions = string.char(
    ucCore0, ucCore1,
    ucMMIO_SCL,
    ucMMIO_SDA,
    ucPSCL0, ucPSCL1,
    ucPSDA0, ucPSDA1
  )

  local tPlugin = tHandle.plugin
  if tPlugin==nil then
    tLog.error('The handle has no "plugin" set.')
  else
    -- Run the command.
    local aParameter = {
      0xffffffff,    -- verbose
      self.I2C_CMD_Open,
      tHandle.ulHandleAddress
    }
    tester:mbin_set_parameter(tPlugin, aAttr, aParameter)
    -- Append the options.
    tester:stdWrite(tPlugin, aAttr.ulParameterStartAddress+0x18, strOptions)

    ulValue = tester:mbin_execute(tPlugin, aAttr, aParameter)
    if ulValue~=0 then
      tLog.error('Failed to open the device.')
      error('Failed to open the device.')
    end
  end
end



function I2CNetx:run_sequence(tHandle, strSequence, sizExpectedRxData)
  local tLog = self.tLog
  local tester = _G.tester
  local tResult

  local aAttr = tHandle.attr

  local sizTxBuffer = string.len(strSequence)
  local pucTxBuffer = tHandle.ulBufferAddress
  local pucRxBuffer = tHandle.ulBufferAddress + sizTxBuffer

  local tPlugin = tHandle.plugin
  if tPlugin==nil then
    tLog.error('The handle has no "plugin" set.')
  else
    -- Download the sequence data.
    tester:stdWrite(tPlugin, pucTxBuffer, strSequence)

    -- Run the command.
    local aParameter = {
      0xffffffff,    -- verbose
      self.I2C_CMD_RunSequence,
      tHandle.ulHandleAddress,
      pucTxBuffer,
      sizTxBuffer,
      pucRxBuffer,
      sizExpectedRxData,
      'OUTPUT'
    }
    tester:mbin_set_parameter(tPlugin, aAttr, aParameter)
    ulValue = tester:mbin_execute(tPlugin, aAttr, aParameter)
    if ulValue~=0 then
      tLog.error('Failed to run the sequence.')
    else
      -- Get the size of the result data from the output parameter.
      local sizResultData = aParameter[7]
      tLog.debug('The netX reports %d bytes of result data.', sizResultData)

      -- Read the result data.
      local strResultData = tester:stdRead(tPlugin, pucRxBuffer, sizResultData)
      tResult = strResultData
    end
  end

  return tResult
end


return I2CNetx
