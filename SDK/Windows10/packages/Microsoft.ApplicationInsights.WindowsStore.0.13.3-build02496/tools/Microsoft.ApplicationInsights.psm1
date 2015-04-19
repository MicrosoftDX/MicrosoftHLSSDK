# .SYNOPSIS
#   Adds Application Insights bootstrapping code to the given EnvDTE.Project.
function Add-ApplicationInsights($project)
{
    if (!$project)
    {
        $project = Get-Project;
    }

    Write-Verbose "Adding Application Insights bootstrapping code to $($project.FullName).";
    $entryPointClassName = Read-ApplicationEntryPoint $project;
    if ($entryPointClassName)
    {
        $entryPointClass = Find-CodeClass $project $entryPointClassName;
        Add-ApplicationInsightsImport $entryPointClass;
        Add-ApplicationInsightsVariable $entryPointClass;
        Add-ApplicationInsightsVariableInitializer $entryPointClass;
    }
}

# Private

Set-Variable applicationInsightsNamespace -option Constant -value "Microsoft.ApplicationInsights";
Set-Variable telemetryClientVariableName -option Constant -value "TelemetryClient";

function Add-ApplicationInsightsImport($codeClass)
{
    $import= Find-CodeImport $codeClass $applicationInsightsNamespace;
    if (!$import)
    {
        $projectItem = $codeClass.ProjectItem;
        Write-Verbose "Adding $applicationInsightsNamespace import to $($projectItem.FileNames(1)).";
        $fileCodeModel = Get-Interface $codeClass.ProjectItem.FileCodeModel -InterfaceType "EnvDTE80.FileCodeModel2";
        if (($codeClass.Language -eq [EnvDTE.CodeModelLanguageConstants]::vsCMLanguageCSharp) -and ($projectItem.DTE.Version -eq "12.0"))
        {
            # Work around the problem in Dev12 C# which does not support additional import parameters required by Visual Basic
            $import = $fileCodeModel.AddImport($applicationInsightsNamespace);
        }
        else
        {
            $import = $fileCodeModel.AddImport($applicationInsightsNamespace, -1);
        }

        # For consistency between C# and VB
        Add-BlankLine $import.EndPoint;
    }
}

function Add-ApplicationInsightsVariable($codeClass)
{
    $telemetryClientVariable = Find-CodeVariable $codeClass $telemetryClientVariableName;
    if (!$telemetryClientVariable)
    {
        Write-Verbose "Adding the $telemetryClientVariableName variable to the $($codeClass.FullName) class in $($codeClass.ProjectItem.FileNames(1)).";
        $telemetryClientVariable = $codeClass.AddVariable($telemetryClientVariableName, "TelemetryClient", 0, [EnvDTE.vsCMAccess]::vsCMAccessPublic);
        $telemetryClientVariable.IsShared = $true;

        $docComment = 
            "<summary>`n" +
            "Allows tracking page views, exceptions and other telemetry through the Microsoft Application Insights service.`n" +
            "</summary>";
        Set-DocComment $telemetryClientVariable $docComment;

        # For consistency between C# and VB
        Add-BlankLine $telemetryClientVariable.EndPoint;
    }
}

function Add-ApplicationInsightsVariableInitializer($codeClass)
{
    $constructor = Find-Constructor $codeClass;
    if (!$constructor)
    {
        $constructor = Add-Constructor $codeClass;
        Add-Statement $constructor "InitializeComponent()";
    }

    $statementText = "$telemetryClientVariableName = new TelemetryClient()";
    $statement = Find-Statement $constructor $statementText;
    if (!$statement)
    {
        # Inject code in the beginning of the constructor to start tracking exceptions as soon as possible
        Write-Verbose "Initializing the $telemetryClientVariableName variable in constructor of $($codeClass.Name).";
        $textPoint = Add-Statement $constructor $statementText;
        Add-BlankLine $textPoint;
    }
}

function Add-Constructor($codeClass)
{
    Write-Verbose "Adding constructor to $($codeClass.FullName) class in $($codeClass.ProjectItem.FileNames(1)).";

    if ($codeClass.Language -eq [EnvDTE.CodeModelLanguageConstants]::vsCMLanguageCSharp)
    {
        $constructor = $codeClass.AddFunction($codeClass.Name, [EnvDTE.vsCMFunction]::vsCMFunctionConstructor, $null, $null, [EnvDTE.vsCMAccess]::vsCMAccessPublic);
    }
    else
    {
        $constructor = $codeClass.AddFunction("New", [EnvDTE.vsCMFunction]::vsCMFunctionSub, $null, $null, [EnvDTE.vsCMAccess]::vsCMAccessPublic);

        # Delete blank lines for consistency with C#
        $constructor = Get-Interface $constructor -InterfaceType "EnvDTE80.CodeFunction2";
        $constructor.
            GetStartPoint([EnvDTE.vsCMPart]::vsCMPartBody).
            CreateEditPoint().
            DeleteWhitespace([EnvDTE.vsWhitespaceOptions]::vsWhitespaceOptionsVertical);
    }

    # Add a doc comment above constructor for consistency with default code templates.
    $docComment = 
        "<summary>`n" +
        "Initializes a new instance of the $($codeClass.Name) class.`n" +
        "</summary>";
    Set-DocComment $constructor $docComment;

    # Add a blank line below the constructor for readability
    Add-BlankLine $constructor.EndPoint;

    return $constructor;
}

function Add-Statement($codeFunction, $statement)
{
    if ($codeFunction.Language -eq [EnvDTE.CodeModelLanguageConstants]::vsCMLanguageCSharp)
    {
        $statement = $statement + ";";
        $indentLevel = 3;
    }
    else
    {
        $indentLevel = 2;
    }
    
    $textPoint = $codeFunction.GetStartPoint([EnvDTE.vsCMPart]::vsCMPartBody);
    $editPoint = $textPoint.CreateEditPoint();
    $editPoint.Insert($statement);

    # Align new statement with the rest of the code in this function
    $editPoint.Indent($editPoint, $indentLevel);

    $editPoint.Insert([Environment]::NewLine);

    # Move text caret to the end of the newly added line
    $editPoint.LineUp();
    $editPoint.EndOfLine();

    return $editPoint;
}

function Add-BlankLine($textPoint)
{
    $editPoint = $textPoint.CreateEditPoint();

    # Delete existing blank lines, if any
    $editPoint.LineDown();
    $editPoint.StartOfLine();
    $editPoint.DeleteWhitespace([EnvDTE.vsWhitespaceOptions]::vsWhitespaceOptionsVertical);

    # Insert a single blank line
    $editPoint.Insert([Environment]::NewLine);
}

function Set-DocComment($codeElement, $docComment)
{
    if ($codeElement.Language -eq [EnvDTE.CodeModelLanguageConstants]::vsCMLanguageCSharp)
    {
        # C# expects DocComment inside of a <doc/> element
        $docComment = "<doc>$docComment</doc>";
    }

    $codeElement.DocComment = $docComment;
}

function Find-CodeImport($codeClass, [string]$namespace)
{
    # Walk from the class up because FileCodeModel.CodeElements is empty in Dev14
    $collection = $codeClass.Collection;
    $parent = $codeClass.Parent;
    while ($parent)
    {
        $import = $collection | Where-Object { ($_ -is [EnvDTE80.CodeImport]) -and ($_.Namespace -eq $applicationInsightsNamespace) };
        if ($import)
        {
            return $import;
        }

        $collection = $parent.Collection;
        $parent = $parent.Parent;
    }
}

function Find-CodeVariable($codeClass, [string]$name)
{
    return Get-CodeElement $codeClass.Members | Where-Object { ($_ -is [EnvDTE.CodeVariable]) -and ($_.Name -eq $name) };
}

function Find-Constructor($codeClass)
{
    return Get-CodeElement $codeClass.Members |
        Foreach-Object { Get-Interface $_ -InterfaceType "EnvDTE80.CodeFunction2" } | 
        Where-Object { $_.FunctionKind -eq [EnvDTE.vsCMFunction]::vsCMFunctionConstructor };
}

function Find-CodeClass($project, [string]$fullName)
{
    $class = $project.CodeModel.CodeTypeFromFullName($fullName);
    if ($class)
    {
        $fileName = $class.ProjectItem.FileNames(1);

        # Work around the problem in Dev14, which sometimes finds classes in generated files instead of those in the project.
        if ($fileName.Contains(".g."))
        {
            Write-Verbose "Class $fullName is found in a generated file $fileName. Trying to work around.";
            $document = $project.DTE.Documents.Open($fileName);
            $document.Close();
            return Find-CodeClass $project $fullName;
        }

        Write-Verbose "Class $fullName is defined in $fileName.";
    }

    return $class;
}

function Find-Statement($codeElement, $statement)
{
    $editPoint = $codeElement.StartPoint.CreateEditPoint();
    $bodyText = $editPoint.GetText($codeElement.EndPoint);
    $indexOfStatement = $bodyText.IndexOf($statement);
    return $indexOfStatement -ge 0;
}

function Get-CodeElement($codeElements)
{
    foreach ($child in $codeElements)
    {
        Write-Output $child;
        Get-CodeElement $child.Members;
    }
}

function Read-ApplicationEntryPoint($project)
{
    # Try getting a Silverlight entry point first because Silverlight 8.1 have a different appxmanifest.
    $entryPoint = Get-SilverlightEntryPoint($project);

    if (!$entryPoint)
    {
        $entryPoint = Get-WindowsRuntimeEntryPoint($project);
    }

    if ($entryPoint)
    {
        Write-Verbose "Application entry point is $entryPoint.";
    }

    return $entryPoint;
}

function Get-WindowsRuntimeEntryPoint($project)
{
    $appxManifestXml = Find-AppxManifestXml $project;
    if ($appxManifestXml)
    {
        $entryPoint = $appxManifestXml.Package.Applications.FirstChild.EntryPoint;

        # for universal projects entry point namespace will be based on assembly name instead of root namespace
        $rootNamespace = Get-PropertyValue $project "RootNamespace";
        $assemblyName = Get-PropertyValue $project "AssemblyName";
        $entryPoint = $entryPoint.Replace($assemblyName, $rootNamespace);
    }

    return $entryPoint;
}

function Get-SilverlightEntryPoint($project)
{
    return Get-PropertyValue $project "WindowsPhoneProject.AppEntryObject";
}

function Find-AppxManifestXml($project)
{
    $projectItem = Find-ProjectItem $project.ProjectItems IsAppxManifest;
    if ($projectItem -ne $null)
    {
        $filePath = $projectItem.FileNames(1);
        Write-Verbose "Loading application manifest from $filePath.";
        $fileContents = Get-Content $filePath;
        return [xml]$fileContents;
    }

    return $null;
}

function IsAppxManifest($projectItem)
{
    $itemType = Get-PropertyValue $projectItem "ItemType";
    return $itemType -eq "AppxManifest";
}

function Find-ProjectItem($projectItems, $isMatch)
{
    foreach ($childItem in $projectItems)
    {
        if (&$isMatch $childItem)
        {
            return $childItem;
        }

        $descendantItem = Find-ProjectItem $childItem.ProjectItems $isMatch;
        if ($descendantItem -ne $null)
        {
            return $descendantItem;
        }
    }

    return $null;
}

function Get-PropertyValue($item, [string]$propertyName)
{
    try
    {
        $value = $item.Properties.Item($propertyName).Value;
    }
    catch [System.ArgumentException], [System.Management.Automation.MethodInvocationException]
    {
    }

    Write-Verbose "$propertyName property of $($item.Name) is <$value>.";
    return $value;
}

# Exports
Export-ModuleMember -Function Add-ApplicationInsights;

# SIG # Begin signature block
# MIIaxwYJKoZIhvcNAQcCoIIauDCCGrQCAQExCzAJBgUrDgMCGgUAMGkGCisGAQQB
# gjcCAQSgWzBZMDQGCisGAQQBgjcCAR4wJgIDAQAABBAfzDtgWUsITrck0sYpfvNR
# AgEAAgEAAgEAAgEAAgEAMCEwCQYFKw4DAhoFAAQUVM3rF4gDS4lYJeOLVkepcLRn
# 3MugghV6MIIEuzCCA6OgAwIBAgITMwAAAFnWc81RjvAixQAAAAAAWTANBgkqhkiG
# 9w0BAQUFADB3MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4G
# A1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMSEw
# HwYDVQQDExhNaWNyb3NvZnQgVGltZS1TdGFtcCBQQ0EwHhcNMTQwNTIzMTcxMzE1
# WhcNMTUwODIzMTcxMzE1WjCBqzELMAkGA1UEBhMCVVMxCzAJBgNVBAgTAldBMRAw
# DgYDVQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24x
# DTALBgNVBAsTBE1PUFIxJzAlBgNVBAsTHm5DaXBoZXIgRFNFIEVTTjpGNTI4LTM3
# NzctOEE3NjElMCMGA1UEAxMcTWljcm9zb2Z0IFRpbWUtU3RhbXAgU2VydmljZTCC
# ASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMZsTs9oU/3vgN7oi8Sx8H4H
# zh487AyMNYdM6VE6vLawlndC+v88z+Ha4on6bkIAmVsW3QlkOOJS+9+O+pOjPbuH
# j264h8nQYE/PnIKRbZEbchCz2EN8WUpgXcawVdAn2/L2vfIgxiIsnmuLLWzqeATJ
# S8FwCee2Ha+ajAY/eHD6du7SJBR2sq4gKIMcqfBIkj+ihfeDysVR0JUgA3nSV7wT
# tU64tGxWH1MeFbvPMD/9OwHNX3Jo98rzmWYzqF0ijx1uytpl0iscJKyffKkQioXi
# bS5cSv1JuXtAsVPG30e5syNOIkcc08G5SXZCcs6Qhg4k9cI8uQk2P6hTXFb+X2EC
# AwEAAaOCAQkwggEFMB0GA1UdDgQWBBRbKBqzzXUNYz39mfWbFQJIGsumrDAfBgNV
# HSMEGDAWgBQjNPjZUkZwCu1A+3b7syuwwzWzDzBUBgNVHR8ETTBLMEmgR6BFhkNo
# dHRwOi8vY3JsLm1pY3Jvc29mdC5jb20vcGtpL2NybC9wcm9kdWN0cy9NaWNyb3Nv
# ZnRUaW1lU3RhbXBQQ0EuY3JsMFgGCCsGAQUFBwEBBEwwSjBIBggrBgEFBQcwAoY8
# aHR0cDovL3d3dy5taWNyb3NvZnQuY29tL3BraS9jZXJ0cy9NaWNyb3NvZnRUaW1l
# U3RhbXBQQ0EuY3J0MBMGA1UdJQQMMAoGCCsGAQUFBwMIMA0GCSqGSIb3DQEBBQUA
# A4IBAQB68A30RWw0lg538OLAQgVh94jTev2I1af193/yCPbV/cvKdHzbCanf1hUH
# mb/QPoeEYnvCBo7Ki2jiPd+eWsWMsqlc/lliJvXX+Xi2brQKkGVm6VEI8XzJo7cE
# N0bF54I+KFzvT3Gk57ElWuVDVDMIf6SwVS3RgnBIESANJoEO7wYldKuFw8OM4hRf
# 6AVUj7qGiaqWrpRiJfmvaYgKDLFRxAnvuIB8U5B5u+mP0EjwYsiZ8WU0O/fOtftm
# mLmiWZldPpWfFL81tPuYciQpDPO6BHqCOftGzfHgsha8fSD4nDkVJaEmLdaLgb3G
# vbCdVP5HC18tTir0h+q1D7W37ZIpMIIE7DCCA9SgAwIBAgITMwAAAMps1TISNcTh
# VQABAAAAyjANBgkqhkiG9w0BAQUFADB5MQswCQYDVQQGEwJVUzETMBEGA1UECBMK
# V2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0
# IENvcnBvcmF0aW9uMSMwIQYDVQQDExpNaWNyb3NvZnQgQ29kZSBTaWduaW5nIFBD
# QTAeFw0xNDA0MjIxNzM5MDBaFw0xNTA3MjIxNzM5MDBaMIGDMQswCQYDVQQGEwJV
# UzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UE
# ChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMQ0wCwYDVQQLEwRNT1BSMR4wHAYDVQQD
# ExVNaWNyb3NvZnQgQ29ycG9yYXRpb24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw
# ggEKAoIBAQCWcV3tBkb6hMudW7dGx7DhtBE5A62xFXNgnOuntm4aPD//ZeM08aal
# IV5WmWxY5JKhClzC09xSLwxlmiBhQFMxnGyPIX26+f4TUFJglTpbuVildGFBqZTg
# rSZOTKGXcEknXnxnyk8ecYRGvB1LtuIPxcYnyQfmegqlFwAZTHBFOC2BtFCqxWfR
# +nm8xcyhcpv0JTSY+FTfEjk4Ei+ka6Wafsdi0dzP7T00+LnfNTC67HkyqeGprFVN
# TH9MVsMTC3bxB/nMR6z7iNVSpR4o+j0tz8+EmIZxZRHPhckJRIbhb+ex/KxARKWp
# iyM/gkmd1ZZZUBNZGHP/QwytK9R/MEBnAgMBAAGjggFgMIIBXDATBgNVHSUEDDAK
# BggrBgEFBQcDAzAdBgNVHQ4EFgQUH17iXVCNVoa+SjzPBOinh7XLv4MwUQYDVR0R
# BEowSKRGMEQxDTALBgNVBAsTBE1PUFIxMzAxBgNVBAUTKjMxNTk1K2I0MjE4ZjEz
# LTZmY2EtNDkwZi05YzQ3LTNmYzU1N2RmYzQ0MDAfBgNVHSMEGDAWgBTLEejK0rQW
# WAHJNy4zFha5TJoKHzBWBgNVHR8ETzBNMEugSaBHhkVodHRwOi8vY3JsLm1pY3Jv
# c29mdC5jb20vcGtpL2NybC9wcm9kdWN0cy9NaWNDb2RTaWdQQ0FfMDgtMzEtMjAx
# MC5jcmwwWgYIKwYBBQUHAQEETjBMMEoGCCsGAQUFBzAChj5odHRwOi8vd3d3Lm1p
# Y3Jvc29mdC5jb20vcGtpL2NlcnRzL01pY0NvZFNpZ1BDQV8wOC0zMS0yMDEwLmNy
# dDANBgkqhkiG9w0BAQUFAAOCAQEAd1zr15E9zb17g9mFqbBDnXN8F8kP7Tbbx7Us
# G177VAU6g3FAgQmit3EmXtZ9tmw7yapfXQMYKh0nfgfpxWUftc8Nt1THKDhaiOd7
# wRm2VjK64szLk9uvbg9dRPXUsO8b1U7Brw7vIJvy4f4nXejF/2H2GdIoCiKd381w
# gp4YctgjzHosQ+7/6sDg5h2qnpczAFJvB7jTiGzepAY1p8JThmURdwmPNVm52Iao
# AP74MX0s9IwFncDB1XdybOlNWSaD8cKyiFeTNQB8UCu8Wfz+HCk4gtPeUpdFKRhO
# lludul8bo/EnUOoHlehtNA04V9w3KDWVOjic1O1qhV0OIhFeezCCBbwwggOkoAMC
# AQICCmEzJhoAAAAAADEwDQYJKoZIhvcNAQEFBQAwXzETMBEGCgmSJomT8ixkARkW
# A2NvbTEZMBcGCgmSJomT8ixkARkWCW1pY3Jvc29mdDEtMCsGA1UEAxMkTWljcm9z
# b2Z0IFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5MB4XDTEwMDgzMTIyMTkzMloX
# DTIwMDgzMTIyMjkzMloweTELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
# b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3Jh
# dGlvbjEjMCEGA1UEAxMaTWljcm9zb2Z0IENvZGUgU2lnbmluZyBQQ0EwggEiMA0G
# CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCycllcGTBkvx2aYCAgQpl2U2w+G9Zv
# zMvx6mv+lxYQ4N86dIMaty+gMuz/3sJCTiPVcgDbNVcKicquIEn08GisTUuNpb15
# S3GbRwfa/SXfnXWIz6pzRH/XgdvzvfI2pMlcRdyvrT3gKGiXGqelcnNW8ReU5P01
# lHKg1nZfHndFg4U4FtBzWwW6Z1KNpbJpL9oZC/6SdCnidi9U3RQwWfjSjWL9y8lf
# RjFQuScT5EAwz3IpECgixzdOPaAyPZDNoTgGhVxOVoIoKgUyt0vXT2Pn0i1i8UU9
# 56wIAPZGoZ7RW4wmU+h6qkryRs83PDietHdcpReejcsRj1Y8wawJXwPTAgMBAAGj
# ggFeMIIBWjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQWBBTLEejK0rQWWAHJNy4z
# Fha5TJoKHzALBgNVHQ8EBAMCAYYwEgYJKwYBBAGCNxUBBAUCAwEAATAjBgkrBgEE
# AYI3FQIEFgQU/dExTtMmipXhmGA7qDFvpjy82C0wGQYJKwYBBAGCNxQCBAweCgBT
# AHUAYgBDAEEwHwYDVR0jBBgwFoAUDqyCYEBWJ5flJRP8KuEKU5VZ5KQwUAYDVR0f
# BEkwRzBFoEOgQYY/aHR0cDovL2NybC5taWNyb3NvZnQuY29tL3BraS9jcmwvcHJv
# ZHVjdHMvbWljcm9zb2Z0cm9vdGNlcnQuY3JsMFQGCCsGAQUFBwEBBEgwRjBEBggr
# BgEFBQcwAoY4aHR0cDovL3d3dy5taWNyb3NvZnQuY29tL3BraS9jZXJ0cy9NaWNy
# b3NvZnRSb290Q2VydC5jcnQwDQYJKoZIhvcNAQEFBQADggIBAFk5Pn8mRq/rb0Cx
# MrVq6w4vbqhJ9+tfde1MOy3XQ60L/svpLTGjI8x8UJiAIV2sPS9MuqKoVpzjcLu4
# tPh5tUly9z7qQX/K4QwXaculnCAt+gtQxFbNLeNK0rxw56gNogOlVuC4iktX8pVC
# nPHz7+7jhh80PLhWmvBTI4UqpIIck+KUBx3y4k74jKHK6BOlkU7IG9KPcpUqcW2b
# Gvgc8FPWZ8wi/1wdzaKMvSeyeWNWRKJRzfnpo1hW3ZsCRUQvX/TartSCMm78pJUT
# 5Otp56miLL7IKxAOZY6Z2/Wi+hImCWU4lPF6H0q70eFW6NB4lhhcyTUWX92THUmO
# Lb6tNEQc7hAVGgBd3TVbIc6YxwnuhQ6MT20OE049fClInHLR82zKwexwo1eSV32U
# jaAbSANa98+jZwp0pTbtLS8XyOZyNxL0b7E8Z4L5UrKNMxZlHg6K3RDeZPRvzkbU
# 0xfpecQEtNP7LN8fip6sCvsTJ0Ct5PnhqX9GuwdgR2VgQE6wQuxO7bN2edgKNAlt
# HIAxH+IOVN3lofvlRxCtZJj/UBYufL8FIXrilUEnacOTj5XJjdibIa4NXJzwoq6G
# aIMMai27dmsAHZat8hZ79haDJLmIz2qoRzEvmtzjcT3XAH5iR9HOiMm4GPoOco3B
# oz2vAkBq/2mbluIQqBC0N1AI1sM9MIIGBzCCA++gAwIBAgIKYRZoNAAAAAAAHDAN
# BgkqhkiG9w0BAQUFADBfMRMwEQYKCZImiZPyLGQBGRYDY29tMRkwFwYKCZImiZPy
# LGQBGRYJbWljcm9zb2Z0MS0wKwYDVQQDEyRNaWNyb3NvZnQgUm9vdCBDZXJ0aWZp
# Y2F0ZSBBdXRob3JpdHkwHhcNMDcwNDAzMTI1MzA5WhcNMjEwNDAzMTMwMzA5WjB3
# MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVk
# bW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMSEwHwYDVQQDExhN
# aWNyb3NvZnQgVGltZS1TdGFtcCBQQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw
# ggEKAoIBAQCfoWyx39tIkip8ay4Z4b3i48WZUSNQrc7dGE4kD+7Rp9FMrXQwIBHr
# B9VUlRVJlBtCkq6YXDAm2gBr6Hu97IkHD/cOBJjwicwfyzMkh53y9GccLPx754gd
# 6udOo6HBI1PKjfpFzwnQXq/QsEIEovmmbJNn1yjcRlOwhtDlKEYuJ6yGT1VSDOQD
# LPtqkJAwbofzWTCd+n7Wl7PoIZd++NIT8wi3U21StEWQn0gASkdmEScpZqiX5NMG
# gUqi+YSnEUcUCYKfhO1VeP4Bmh1QCIUAEDBG7bfeI0a7xC1Un68eeEExd8yb3zuD
# k6FhArUdDbH895uyAc4iS1T/+QXDwiALAgMBAAGjggGrMIIBpzAPBgNVHRMBAf8E
# BTADAQH/MB0GA1UdDgQWBBQjNPjZUkZwCu1A+3b7syuwwzWzDzALBgNVHQ8EBAMC
# AYYwEAYJKwYBBAGCNxUBBAMCAQAwgZgGA1UdIwSBkDCBjYAUDqyCYEBWJ5flJRP8
# KuEKU5VZ5KShY6RhMF8xEzARBgoJkiaJk/IsZAEZFgNjb20xGTAXBgoJkiaJk/Is
# ZAEZFgltaWNyb3NvZnQxLTArBgNVBAMTJE1pY3Jvc29mdCBSb290IENlcnRpZmlj
# YXRlIEF1dGhvcml0eYIQea0WoUqgpa1Mc1j0BxMuZTBQBgNVHR8ESTBHMEWgQ6BB
# hj9odHRwOi8vY3JsLm1pY3Jvc29mdC5jb20vcGtpL2NybC9wcm9kdWN0cy9taWNy
# b3NvZnRyb290Y2VydC5jcmwwVAYIKwYBBQUHAQEESDBGMEQGCCsGAQUFBzAChjho
# dHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpL2NlcnRzL01pY3Jvc29mdFJvb3RD
# ZXJ0LmNydDATBgNVHSUEDDAKBggrBgEFBQcDCDANBgkqhkiG9w0BAQUFAAOCAgEA
# EJeKw1wDRDbd6bStd9vOeVFNAbEudHFbbQwTq86+e4+4LtQSooxtYrhXAstOIBNQ
# md16QOJXu69YmhzhHQGGrLt48ovQ7DsB7uK+jwoFyI1I4vBTFd1Pq5Lk541q1YDB
# 5pTyBi+FA+mRKiQicPv2/OR4mS4N9wficLwYTp2OawpylbihOZxnLcVRDupiXD8W
# mIsgP+IHGjL5zDFKdjE9K3ILyOpwPf+FChPfwgphjvDXuBfrTot/xTUrXqO/67x9
# C0J71FNyIe4wyrt4ZVxbARcKFA7S2hSY9Ty5ZlizLS/n+YWGzFFW6J1wlGysOUzU
# 9nm/qhh6YinvopspNAZ3GmLJPR5tH4LwC8csu89Ds+X57H2146SodDW4TsVxIxIm
# dgs8UoxxWkZDFLyzs7BNZ8ifQv+AeSGAnhUwZuhCEl4ayJ4iIdBD6Svpu/RIzCzU
# 2DKATCYqSCRfWupW76bemZ3KOm+9gSd0BhHudiG/m4LBJ1S2sWo9iaF2YbRuoROm
# v6pH8BJv/YoybLL+31HIjCPJZr2dHYcSZAI9La9Zj7jkIeW1sMpjtHhUBdRBLlCs
# lLCleKuzoJZ1GtmShxN1Ii8yqAhuoFuMJb+g74TKIdbrHk/Jmu5J4PcBZW+JC33I
# acjmbuqnl84xKf8OxVtc2E0bodj6L54/LlUWa8kTo/0xggS3MIIEswIBATCBkDB5
# MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMHUmVk
# bW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMSMwIQYDVQQDExpN
# aWNyb3NvZnQgQ29kZSBTaWduaW5nIFBDQQITMwAAAMps1TISNcThVQABAAAAyjAJ
# BgUrDgMCGgUAoIHQMBkGCSqGSIb3DQEJAzEMBgorBgEEAYI3AgEEMBwGCisGAQQB
# gjcCAQsxDjAMBgorBgEEAYI3AgEVMCMGCSqGSIb3DQEJBDEWBBQzO5ho65l4X0n2
# 9QoNJ6yxvK1USzBwBgorBgEEAYI3AgEMMWIwYKBGgEQATQBpAGMAcgBvAHMAbwBm
# AHQALgBBAHAAcABsAGkAYwBhAHQAaQBvAG4ASQBuAHMAaQBnAGgAdABzAC4AcABz
# AG0AMaEWgBRodHRwOi8vbWljcm9zb2Z0LmNvbTANBgkqhkiG9w0BAQEFAASCAQBR
# 0HlvJaiC73Crx0cMkHrBEc4soMVSFV+uIgw+B62LI8DwPXuZTr7XbSLs2CCLQl9S
# qeMI1SJDg+hEou/Wp8Xo2V5ysuu8UCN6bVWAmHpVIiWJbirKYYmRep7rlTKTMRu0
# +U843wLp2BtZ7/sy7vEEHdu6iZHX9DHVA5+1TpuyXP3xSy2QrZs4pFPUUpDRpJtV
# xXQ1kdJxaQEm4ICoy0YeMk3LYH7Wr7dVmpgKjs/jI+9B7vs1d6zUTbjPcfoQgeYO
# WpQ7zn12oIY3LRpUcgPyVEd4Kkc3jQC0AbptZtcMnrIPN97rwzwTFsAdq9BNudSU
# rHT2ANZeQHuu64JV9QY/oYICKDCCAiQGCSqGSIb3DQEJBjGCAhUwggIRAgEBMIGO
# MHcxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQHEwdS
# ZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29ycG9yYXRpb24xITAfBgNVBAMT
# GE1pY3Jvc29mdCBUaW1lLVN0YW1wIFBDQQITMwAAAFnWc81RjvAixQAAAAAAWTAJ
# BgUrDgMCGgUAoF0wGAYJKoZIhvcNAQkDMQsGCSqGSIb3DQEHATAcBgkqhkiG9w0B
# CQUxDxcNMTUwMzA2MDAxNzMwWjAjBgkqhkiG9w0BCQQxFgQUScq0iiVn/O3pKmrS
# LrzTawQeCbMwDQYJKoZIhvcNAQEFBQAEggEAopxg6+QJ+3kCmIV3RoAyzZFoyjnO
# Nxb2fxXQvaPrMurhdnpsLqOML2vLsX6ySU3bH2z7a03SfIde4lXfSO5PmlAuNdqa
# Idfl9OSleHnA9hm6BgJlQXi7YhZDakIH34P+QGQ5UM0Sckc/jHcNSSpEYpC/u6xS
# bNcu7w70G3+yOJLSQ7QJp0Otska2woT2cBrteQZiGwWF7CSBQzqWLPTmmgLsIc0m
# bu7EP5Oju/VCTfeKRuDsZ12HdEQWD/Eq9zNBNYT2wmZLS4ns2L2x/wxviGNskslq
# 8nNHGmglvj8ogeekXcN1wV+QG0ZCbbNokPT5VK2CvMcNABAlmk7pqvZbQQ==
# SIG # End signature block
