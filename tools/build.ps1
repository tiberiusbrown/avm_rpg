param([Alias('SdkRoot')][string]$sdk_root = '', [Alias('BuildDir')][string]$build_dir = 'build', [Alias('HostOnly')][switch]$host_only)
$ErrorActionPreference = 'Stop'
$rpg_root = Split-Path $PSScriptRoot -Parent
if(-not $sdk_root -and -not $host_only) {
    $rpg_clang = (Get-Command avm-clang -ErrorAction Stop).Source
    $sdk_root = Split-Path (Split-Path $rpg_clang -Parent) -Parent
}
$rpg_build = Join-Path $rpg_root $build_dir
$rpg_cache = Join-Path $rpg_build 'CMakeCache.txt'
$rpg_parent_cache = Join-Path $rpg_root '../../build/CMakeCache.txt'
$rpg_vcvars = $null
if(-not (Get-Command cl -ErrorAction SilentlyContinue)) {
    foreach($rpg_candidate in @($rpg_cache, $rpg_parent_cache)) {
        if(Test-Path -LiteralPath $rpg_candidate) {
            $rpg_match = Select-String -LiteralPath $rpg_candidate -Pattern '^CMAKE_C(?:XX)?_COMPILER:FILEPATH=(.*cl.exe)$' | Select-Object -First 1
            if($rpg_match) {
                $rpg_compiler = $rpg_match.Matches[0].Groups[1].Value.Replace('\', '/')
                $rpg_vcvars = Join-Path $rpg_compiler.Split('/VC/')[0] 'VC/Auxiliary/Build/vcvars64.bat'
                break
            }
        }
    }
    if(-not $rpg_vcvars) {
        $rpg_vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $rpg_vs = & $rpg_vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if(-not $rpg_vs) { throw 'Install MSVC C++ tools or use a developer shell.' }
        $rpg_vcvars = Join-Path $rpg_vs 'VC/Auxiliary/Build/vcvars64.bat'
    }
}
$rpg_guest = if($host_only) { 'OFF' } else { 'ON' }
$rpg_configure = 'cmake -S "' + $rpg_root + '" -B "' + $rpg_build + '" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRPG_BUILD_GUEST=' + $rpg_guest
if(-not (Test-Path -LiteralPath $rpg_cache)) { $rpg_configure += ' -G Ninja' }
if($sdk_root) { $rpg_configure += ' -DAVM_SDK_ROOT="' + $sdk_root + '"' }
$rpg_command = $rpg_configure + ' && cmake --build "' + $rpg_build + '" --config RelWithDebInfo --parallel 4 && ctest --test-dir "' + $rpg_build + '" -C RelWithDebInfo --output-on-failure'
if($rpg_vcvars) { $rpg_command = 'call "' + $rpg_vcvars + '" >nul && ' + $rpg_command }
& cmd.exe /d /c $rpg_command
if($LASTEXITCODE) { throw "Build/tests failed ($LASTEXITCODE)" }
