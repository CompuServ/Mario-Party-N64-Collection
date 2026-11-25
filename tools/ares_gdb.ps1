<#
Talks to the ares emulator's GDB server (Settings > Debug > enable the server; port 9123 by default) to read or write
N64 memory while a game runs: handy for finding or checking an address in docs\ADDRESSES.md against the real game.

  powershell -File tools\ares_gdb.ps1 -Read 0x800ED140 -Length 8            # hex bytes
  powershell -File tools\ares_gdb.ps1 -Write 0x800ED140 -Hex 000003E7       # big-endian bytes, as the N64 has them
  powershell -File tools\ares_gdb.ps1 -Packet "?"                           # any raw GDB packet

ares halts the game while a client asks it something; the script always ends with D (detach), which resumes it.
ares drops a client whose first byte isn't '+' (its guard against web pages), so the script sends that first.
#>
param(
    [string]$Read,
    [int]$Length = 4,
    [string]$Write,
    [string]$Hex,
    [string]$Packet,
    [string]$Server = "127.0.0.1",
    [int]$Port = 9123
)
function Send-Text([string]$text) {
    $bytes = [Text.Encoding]::ASCII.GetBytes($text)
    $stream.Write($bytes, 0, $bytes.Length)
}

# One reply packet ($...#xx), acknowledged; '+' acks before it are dropped.
function Read-Reply([int]$timeoutMs = 3000) {
    $buffer = New-Object byte[] 65536
    $text = ""
    $end = [DateTime]::Now.AddMilliseconds($timeoutMs)
    while ([DateTime]::Now -lt $end) {
        if ($stream.DataAvailable) {
            $count = $stream.Read($buffer, 0, $buffer.Length)
            if ($count -eq 0) { break }
            $text += [Text.Encoding]::ASCII.GetString($buffer, 0, $count)
            if ($text -match '\$([^#]*)#[0-9a-fA-F]{2}') { Send-Text "+"; return $Matches[1] }
        } else {
            Start-Sleep -Milliseconds 10
        }
    }
    return $null
}

function Invoke-Packet([string]$command) {
    $sum = 0
    foreach ($b in [Text.Encoding]::ASCII.GetBytes($command)) { $sum = ($sum + $b) % 256 }
    Send-Text ('$' + $command + '#' + ('{0:x2}' -f $sum))
    return Read-Reply
}

function Format-Address([string]$value) {
    return ('{0:x8}' -f [Convert]::ToUInt32(($value -replace '^0x', ''), 16))
}

# ares often kicks the first client after one has left (its kick flag outlives the old client), so try a few times.
$client = $null
for ($try = 1; $try -le 4 -and $null -eq $client; $try++) {
    $candidate = New-Object Net.Sockets.TcpClient
    try {
        $candidate.Connect($Server, $Port)
        $stream = $candidate.GetStream()
        Send-Text "+"
        if ($null -ne (Invoke-Packet "qSupported:swbreak+;hwbreak+")) { $client = $candidate }
    } catch { }
    if ($null -eq $client) { $candidate.Close(); Start-Sleep -Milliseconds 500 }
}
if ($null -eq $client) { throw "No answer from the ares GDB server at ${Server}:${Port}." }

try {
    if ($Read) {
        $reply = Invoke-Packet ("m{0},{1:x}" -f (Format-Address $Read), $Length)
        "$Read : $reply"
    }
    if ($Write) {
        $data = $Hex -replace '\s', ''
        $reply = Invoke-Packet ("M{0},{1:x}:{2}" -f (Format-Address $Write), ($data.Length / 2), $data)
        "$Write <- $data : $reply"
    }
    if ($Packet) {
        "$Packet : $(Invoke-Packet $Packet)"
    }
} finally {
    Send-Text ('$D#44')
    Start-Sleep -Milliseconds 200
    $client.Close()
}
