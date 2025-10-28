# ToyC Compiler Test Runner for PowerShell
param(
    [switch]$BuildOnly,
    [switch]$Verbose,
    [switch]$UnifiedTest
)

$ErrorActionPreference = "Stop"

function Write-Info {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-Host $Message -ForegroundColor Yellow
}

# Check if we're on Windows
$isWindows = $true  # Force Windows mode since this is a PowerShell script

# Build unified test if requested
if ($UnifiedTest) {
    Write-Info "Building unified test program..."
    try {
        if ($isWindows) {
            g++ -std=c++20 -O2 -Wall src/unified_test.cpp src/ra_linear_scan.cpp -o unified_test.exe
        }
        else {
            g++ -std=c++20 -O2 -Wall src/unified_test.cpp src/ra_linear_scan.cpp -o unified_test
        }
        Write-Info "Unified test program built successfully!"
        Write-Info "You can now run the unified test program:"
        if ($isWindows) {
            Write-Info "  ./unified_test.exe"
        }
        else {
            Write-Info "  ./unified_test"
        }
        exit 0
    }
    catch {
        Write-Error "Failed to build unified test: $_"
        exit 1
    }
}

Write-Info "Building ToyC compiler..."

# Build the compiler
try {
    if ($isWindows) {
        # Filter out unified_test.cpp from compilation
        $sourceFiles = Get-ChildItem src/*.cpp | Where-Object { $_.Name -ne "unified_test.cpp" } | ForEach-Object { $_.FullName }
        & g++ -std=c++20 -O2 -Wall -o toyc.exe $sourceFiles
    }
    else {
        # Use make for non-Windows systems
        make build
    }
    Write-Info "Compiler built successfully!"
}
catch {
    Write-Error "Failed to build compiler: $_"
    exit 1
}

if ($BuildOnly) {
    Write-Info "Build completed. Exiting due to -BuildOnly flag."
    exit 0
}

Write-Info "Running tests..."

# Check if scripts exist
$asmScript = "scripts/generate_asm.sh"
$irScript = "scripts/generate_ir.sh"

if (-not (Test-Path $asmScript)) {
    Write-Error "Script not found: $asmScript"
    exit 1
}

if (-not (Test-Path $irScript)) {
    Write-Error "Script not found: $irScript"
    exit 1
}

# Run tests using WSL on Windows, or bash directly on Linux/macOS
try {
    if ($isWindows) {
        Write-Info "Running assembly generation tests..."
        wsl bash $asmScript
        if ($LASTEXITCODE -ne 0) {
            throw "Assembly generation failed with exit code $LASTEXITCODE"
        }
        
        Write-Info "Running IR generation tests..."
        wsl bash $irScript
        if ($LASTEXITCODE -ne 0) {
            throw "IR generation failed with exit code $LASTEXITCODE"
        }
    }
    else {
        Write-Info "Running assembly generation tests..."
        bash $asmScript
        if ($LASTEXITCODE -ne 0) {
            throw "Assembly generation failed with exit code $LASTEXITCODE"
        }
        
        Write-Info "Running IR generation tests..."
        bash $irScript
        if ($LASTEXITCODE -ne 0) {
            throw "IR generation failed with exit code $LASTEXITCODE"
        }
    }
    
    Write-Info "All tests completed successfully!"
}
catch {
    Write-Error "Test execution failed: $_"
    exit 1
}
