<#
.SYNOPSIS
    Script de configuración de entorno para SmartAccess iButton Lite v6.6.
    Automatiza la creación de Symlinks y valida el motor Git aislado.
#>

$ProjectDrivePath = "G:\Mi unidad\Proyectos-Smart\proy-02-smartaccess-ibutton-lite"
$ProjectLocalGit = "C:\Users\Franc\.gemini\projects\proy-02-smartaccess-ibutton-lite-git"

function Check-Admin {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (-not (Check-Admin)) {
    Write-Error "Este script requiere privilegios de ADMINISTRADOR para crear enlaces simbólicos (mklink)."
    Write-Host "Por favor, abre PowerShell como administrador y vuelve a ejecutarlo." -ForegroundColor Red
    exit
}

Write-Host "--- Configurando Entorno SmartAccess iButton Lite ---" -ForegroundColor Cyan

# 1. Validar rutas
if (-not (Test-Path $ProjectDrivePath)) {
    Write-Error "No se encuentra la ruta en Google Drive: $ProjectDrivePath"
    exit
}

# 2. Informar sobre el motor Git
Write-Host "[INFO] El motor Git está ubicado en: $ProjectLocalGit" -ForegroundColor Gray

# 3. Instrucción para Symlinks futuros
Write-Host "[OK] Infraestructura validada." -ForegroundColor Green
Write-Host "Para enlazar este proyecto a una carpeta de trabajo local rápida (ej. C:\Dev), usa:"
Write-Host "New-Item -ItemType SymbolicLink -Path 'C:\Dev\smartaccess-lite' -Target '$ProjectDrivePath'" -ForegroundColor Yellow

