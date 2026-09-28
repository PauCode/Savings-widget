# Savings Goals Management Script
# This script provides functions for managing savings goals and jars with delete capabilities

param(
    [string]$Action = "list",
    [string]$GoalId = "",
    [string]$Name = "",
    [string]$TargetAmount = "",
    [string]$Currency = ""
)

# Function to initialize the goals data directory
function Initialize-GoalsData {
    $goalsPath = "current/goals"
    if (!(Test-Path $goalsPath)) {
        New-Item -ItemType Directory -Path $goalsPath -Force
    }
    
    $metaPath = "current/meta"
    if (!(Test-Path $metaPath)) {
        New-Item -ItemType Directory -Path $metaPath -Force
    }
}

# Function to list all goals
function Get-AllGoals {
    $goalsDir = "current/goals"
    if (Test-Path $goalsDir) {
        $goalFiles = Get-ChildItem -Path $goalsDir -Filter "*.json" | Sort-Object Name
        
        Write-Host "`n=== SAVINGS GOALS ===`n"
        
        if ($null -eq $goalFiles -or $goalFiles.Count -eq 0) {
            Write-Host "No goals found. Create a new goal using 'create' action."
        } else {
            foreach ($file in $goalFiles) {
                try {
                    $content = Get-Content $file.FullName | ConvertFrom-Json
                    Write-Host "ID: $($content.id)"
                    Write-Host "Name: $($content.name)"
                    Write-Host "Target Amount: $($content.targetAmount) $($content.currency)"
                    Write-Host "Current Amount: $($content.currentAmount) $($content.currency)"
                    Write-Host "Status: $(if ($content.currentAmount -ge $content.targetAmount) { 'ACHIEVED' } else { 'IN PROGRESS' })"
                    Write-Host "Created: $($content.createdDate)"
                    Write-Host "-"
                }
                catch {
                    Write-Host "Error reading goal file $($file.Name): $_"
                }
            }
        }
    }
}

# Function to create a new goal
function New-Goal {
    param(
        [string]$Name,
        [double]$TargetAmount,
        [string]$Currency = "RON"
    )
    
    $id = Get-Random -Minimum 100000 -Maximum 999999
    
    # Create the goal object
    $goal = @{
        id = $id.ToString()
        name = $Name
        targetAmount = $TargetAmount
        currentAmount = 0.0
        currency = $Currency
        createdDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        lastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        deleted = $false
    }
    
    # Convert to JSON and save to file
    $goalJson = $goal | ConvertTo-Json
    $filePath = "current/goals/goal-$($id).json"
    
    try {
        Set-Content -Path $filePath -Value $goalJson
        Write-Host "Goal '$Name' created successfully with ID: $($id)"
    }
    catch {
        Write-Error "Failed to create goal: $_"
    }
}

# Function to delete a goal by ID (soft delete)
function Remove-Goal {
    param(
        [string]$GoalId
    )
    
    $filePath = "current/goals/goal-$($GoalId).json"
    
    if (Test-Path $filePath) {
        try {
            # Load the existing goal data
            $goalData = Get-Content -Path $filePath | ConvertFrom-Json
            
            # Mark as deleted instead of actually deleting to preserve ID integrity
            $goalData.deleted = $true
            $goalData.lastUpdated = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            
            # Save the updated data
            $goalData | ConvertTo-Json | Set-Content -Path $filePath
            
            Write-Host "Goal with ID '$($GoalId)' has been marked as deleted."
        }
        catch {
            Write-Error "Failed to delete goal: $_"
        }
    } else {
        Write-Host "Goal with ID '$($GoalId)' not found."
    }
}