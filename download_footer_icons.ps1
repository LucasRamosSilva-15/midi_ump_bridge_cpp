$icons = @{
    'arrow_right_alt'='#555f6d'
    'monitor'='#555f6d'
    'hard_drive'='#555f6d'
}
mkdir assets\icons -Force | Out-Null
foreach ($name in $icons.Keys) {
    $color = $icons[$name]
    $url = "https://raw.githubusercontent.com/marella/material-design-icons/main/svg/outlined/${name}.svg"
    try {
        $svg = (Invoke-WebRequest $url).Content
        $svg = $svg -replace '<svg ', "<svg fill=`"$color`" "
        Set-Content -Path "assets\icons\${name}.svg" -Value $svg -Encoding UTF8
        Write-Host "Downloaded $name"
    } catch {
        Write-Host "Failed to download $name"
    }
}
