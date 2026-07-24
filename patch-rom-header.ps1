param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,

    [Parameter(Mandatory = $true)]
    [ValidateLength(1, 16)]
    [string]$Title
)

$RomBytes = [System.IO.File]::ReadAllBytes($RomPath)
$TitleBytes = [System.Text.Encoding]::ASCII.GetBytes($Title)
for ($i = 0; $i -lt 16; $i++) {
    $RomBytes[0x134 + $i] = if ($i -lt $TitleBytes.Length) { $TitleBytes[$i] } else { 0 }
}

$HeaderChecksum = 0
for ($i = 0x134; $i -le 0x14C; $i++) {
    $HeaderChecksum = ($HeaderChecksum - $RomBytes[$i] - 1) -band 0xFF
}
$RomBytes[0x14D] = [byte]$HeaderChecksum

$GlobalChecksum = 0
for ($i = 0; $i -lt $RomBytes.Length; $i++) {
    if (($i -ne 0x14E) -and ($i -ne 0x14F)) {
        $GlobalChecksum = ([int]$GlobalChecksum + [int]$RomBytes[$i]) -band 0xFFFF
    }
}
$RomBytes[0x14E] = [byte](($GlobalChecksum -shr 8) -band 0xFF)
$RomBytes[0x14F] = [byte]($GlobalChecksum -band 0xFF)
[System.IO.File]::WriteAllBytes($RomPath, $RomBytes)

Write-Host "ROM title: $Title"