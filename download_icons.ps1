$icons = @{
    'settings_input_component'='#555f6d'
    'refresh'='#161c21'
    'cable'='#ffffff'
    'power_off'='#ffffff'
    'linear_scale'='#555f6d'
    'speed'='#161c21'
    'analytics'='#555f6d'
    'arrow_drop_down'='#161c21'
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
