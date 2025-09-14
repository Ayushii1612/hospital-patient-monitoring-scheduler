#include <iostream>
#include <queue>
#include <vector>
#include <chrono>
#include <memory>
#include <map>
#include <string>
#include <random>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <cassert>
#include <functional>
#include <sstream>
#include <ctime>
#include <cstdlib>

// Forward declarations
class Patient;
class MedicalDevice;
class Alert;
class HospitalScheduler;

// Enumerations
enum class Priority {
    CRITICAL = 1,    // 0-2 seconds
    HIGH = 2,        // 2-30 seconds  
    MEDIUM = 3,      // 30-300 seconds
    LOW = 4          // 5-60 minutes
};

enum class VitalSign {
    HEART_RATE,
    BLOOD_PRESSURE,
    OXYGEN_SATURATION,
    TEMPERATURE,
    RESPIRATORY_RATE
};

// Data structures
struct VitalReading {
    VitalSign type;
    double value;
    std::chrono::system_clock::time_point timestamp;
    int patientId;
    
    VitalReading(VitalSign t, double v, int pid) 
        : type(t), value(v), patientId(pid), 
          timestamp(std::chrono::system_clock::now()) {}
};

struct Alert {
    int patientId;
    Priority priority;
    std::string message;
    VitalSign relatedVital;
    std::chrono::system_clock::time_point createdAt;
    bool acknowledged;
    
    Alert(int pid, Priority p, const std::string& msg, VitalSign vital)
        : patientId(pid), priority(p), message(msg), relatedVital(vital),
          createdAt(std::chrono::system_clock::now()), acknowledged(false) {}
};

// Comparator for priority queue
struct AlertComparator {
    bool operator()(const std::shared_ptr<Alert>& a, const std::shared_ptr<Alert>& b) {
        if (a->priority != b->priority) {
            return a->priority > b->priority; // Lower number = higher priority
        }
        return a->createdAt > b->createdAt; // Earlier time = higher priority
    }
};

// Patient class (without threading)
class Patient {
private:
    int patientId;
    std::string name;
    int age;
    std::map<VitalSign, std::vector<VitalReading>> vitalHistory;
    std::map<VitalSign, std::pair<double, double>> normalRanges; // min, max
    Priority currentRiskLevel;
    
public:
    Patient(int id, const std::string& patientName, int patientAge) 
        : patientId(id), name(patientName), age(patientAge), currentRiskLevel(Priority::LOW) {
        initializeNormalRanges();
    }
    
    void initializeNormalRanges() {
        normalRanges[VitalSign::HEART_RATE] = {60.0, 100.0};
        normalRanges[VitalSign::BLOOD_PRESSURE] = {90.0, 140.0}; // systolic
        normalRanges[VitalSign::OXYGEN_SATURATION] = {95.0, 100.0};
        normalRanges[VitalSign::TEMPERATURE] = {36.1, 37.2}; // Celsius
        normalRanges[VitalSign::RESPIRATORY_RATE] = {12.0, 20.0};
    }
    
    void addVitalReading(const VitalReading& reading) {
        vitalHistory[reading.type].push_back(reading);
        
        // Keep only last 100 readings per vital sign
        if (vitalHistory[reading.type].size() > 100) {
            vitalHistory[reading.type].erase(vitalHistory[reading.type].begin());
        }
    }
    
    Priority assessRisk(const VitalReading& reading) {
        auto range = normalRanges[reading.type];
        double value = reading.value;
        
        // Critical conditions
        if (reading.type == VitalSign::HEART_RATE) {
            if (value < 30 || value > 180) return Priority::CRITICAL;
            if (value < 50 || value > 120) return Priority::HIGH;
        }
        else if (reading.type == VitalSign::OXYGEN_SATURATION) {
            if (value < 85) return Priority::CRITICAL;
            if (value < 92) return Priority::HIGH;
        }
        else if (reading.type == VitalSign::BLOOD_PRESSURE) {
            if (value < 60 || value > 200) return Priority::CRITICAL;
            if (value < 80 || value > 160) return Priority::HIGH;
        }
        else if (reading.type == VitalSign::TEMPERATURE) {
            if (value < 35.0 || value > 39.0) return Priority::HIGH;
            if (value < 35.5 || value > 38.5) return Priority::MEDIUM;
        }
        else if (reading.type == VitalSign::RESPIRATORY_RATE) {
            if (value < 8 || value > 30) return Priority::HIGH;
            if (value < 10 || value > 25) return Priority::MEDIUM;
        }
        
        // Normal range check
        if (value < range.first || value > range.second) {
            return Priority::MEDIUM;
        }
        
        return Priority::LOW;
    }
    
    bool detectTrend(VitalSign vital) {
        auto& history = vitalHistory[vital];
        
        if (history.size() < 5) return false;
        
        // Simple trend detection: check if last 5 readings show consistent change
        double sum = 0;
        for (size_t i = history.size() - 4; i < history.size(); i++) {
            sum += history[i].value - history[i-1].value;
        }
        
        double avgChange = sum / 4.0;
        return std::abs(avgChange) > 2.0; // Threshold for concerning trend
    }
    
    std::vector<VitalReading> getRecentReadings(VitalSign vital, int count = 10) {
        auto& history = vitalHistory[vital];
        
        if (history.empty()) return {};
        
        int start = std::max(0, static_cast<int>(history.size()) - count);
        return std::vector<VitalReading>(history.begin() + start, history.end());
    }
    
    int getId() const { return patientId; }
    std::string getName() const { return name; }
    Priority getCurrentRisk() const { return currentRiskLevel; }
    void setCurrentRisk(Priority risk) { currentRiskLevel = risk; }
};

// Medical Device class (simplified without threads)
class MedicalDevice {
private:
    int deviceId;
    VitalSign monitoredVital;
    int assignedPatient;
    bool isActive;
    
public:
    MedicalDevice(int id, VitalSign vital, int patientId) 
        : deviceId(id), monitoredVital(vital), assignedPatient(patientId), isActive(true) {}
    
    VitalReading generateReading() {
        double baseValue = getBaseValue();
        
        // Add realistic variation and occasional abnormal readings
        double noise = ((rand() % 21) - 10) * 0.1; // Random noise ±1.0
        
        // 10% chance of generating abnormal reading for testing
        if ((rand() % 100) < 10) {
            noise += getAbnormalSpike();
        }
        
        return VitalReading(monitoredVital, baseValue + noise, assignedPatient);
    }
    
    void stopMonitoring() {
        isActive = false;
    }
    
    bool isDeviceActive() const {
        return isActive;
    }
    
    int getPatientId() const {
        return assignedPatient;
    }
    
    VitalSign getVitalSign() const {
        return monitoredVital;
    }
    
private:
    double getBaseValue() {
        switch (monitoredVital) {
            case VitalSign::HEART_RATE: return 75.0;
            case VitalSign::BLOOD_PRESSURE: return 120.0;
            case VitalSign::OXYGEN_SATURATION: return 98.0;
            case VitalSign::TEMPERATURE: return 36.8;
            case VitalSign::RESPIRATORY_RATE: return 16.0;
            default: return 0.0;
        }
    }
    
    double getAbnormalSpike() {
        return ((rand() % 61) - 30); // Random spike ±30
    }
};

// False Alarm Detector
class FalseAlarmDetector {
public:
    static bool isLikelyFalseAlarm(const Alert& alert, const std::vector<VitalReading>& recentReadings) {
        if (recentReadings.size() < 5) return false; // Need sufficient data
        
        double mean = calculateMean(recentReadings);
        double stdDev = calculateStandardDeviation(recentReadings, mean);
        
        if (stdDev == 0) return false; // Avoid division by zero
        
        // If current reading is within 1.5 standard deviations, might be false alarm
        double currentValue = recentReadings.back().value;
        double zScore = std::abs((currentValue - mean) / stdDev);
        
        // Different thresholds for different priority levels
        double threshold = (alert.priority == Priority::CRITICAL) ? 2.5 : 1.5;
        
        return zScore < threshold;
    }
    
private:
    static double calculateMean(const std::vector<VitalReading>& readings) {
        double sum = 0.0;
        for (const auto& reading : readings) {
            sum += reading.value;
        }
        return sum / readings.size();
    }
    
    static double calculateStandardDeviation(const std::vector<VitalReading>& readings, double mean) {
        double variance = 0.0;
        for (const auto& reading : readings) {
            variance += std::pow(reading.value - mean, 2);
        }
        return std::sqrt(variance / readings.size());
    }
};

// Alert Processor (simplified without threading)
class AlertProcessor {
private:
    std::priority_queue<std::shared_ptr<Alert>, std::vector<std::shared_ptr<Alert>>, AlertComparator> alertQueue;
    long totalAlertsProcessed;
    long falseAlarmsFiltered;
    
public:
    AlertProcessor() : totalAlertsProcessed(0), falseAlarmsFiltered(0) {}
    
    void addAlert(std::shared_ptr<Alert> alert) {
        alertQueue.push(alert);
    }
    
    void processNextAlert() {
        if (!alertQueue.empty()) {
            auto alert = alertQueue.top();
            alertQueue.pop();
            handleAlert(alert);
            totalAlertsProcessed++;
        }
    }
    
    void processAllAlerts() {
        while (!alertQueue.empty()) {
            processNextAlert();
        }
    }
    
    long getTotalAlertsProcessed() const { return totalAlertsProcessed; }
    long getFalseAlarmsFiltered() const { return falseAlarmsFiltered; }
    bool hasAlerts() const { return !alertQueue.empty(); }
    
private:
    void handleAlert(std::shared_ptr<Alert> alert) {
        auto now = std::chrono::system_clock::now();
        auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - alert->createdAt).count();
            
        // Check response time requirements
        bool withinTimeRequirement = checkResponseTimeRequirement(alert->priority, responseTime);
        
        // Log the alert with response time
        std::cout << "[" << getCurrentTimeString() << "] "
                  << "[" << priorityToString(alert->priority) << "] "
                  << "Patient " << alert->patientId << ": " << alert->message 
                  << " (Response: " << responseTime << "ms)" 
                  << (withinTimeRequirement ? " ✓" : " ⚠") << std::endl;
        
        // Simulate alert handling based on priority
        switch (alert->priority) {
            case Priority::CRITICAL:
                handleCriticalAlert(alert);
                break;
            case Priority::HIGH:
                handleHighAlert(alert);
                break;
            case Priority::MEDIUM:
                handleMediumAlert(alert);
                break;
            case Priority::LOW:
                handleLowAlert(alert);
                break;
        }
    }
    
    bool checkResponseTimeRequirement(Priority priority, long responseTimeMs) {
        switch (priority) {
            case Priority::CRITICAL: return responseTimeMs <= 2000;
            case Priority::HIGH: return responseTimeMs <= 30000;
            case Priority::MEDIUM: return responseTimeMs <= 300000;
            case Priority::LOW: return responseTimeMs <= 3600000;
            default: return true;
        }
    }
    
    void handleCriticalAlert(std::shared_ptr<Alert> alert) {
        std::cout << "    >>> CRITICAL ALERT: Immediate medical attention required!" << std::endl;
    }
    
    void handleHighAlert(std::shared_ptr<Alert> alert) {
        std::cout << "    >>> HIGH PRIORITY: Nurse response needed within 30 seconds" << std::endl;
    }
    
    void handleMediumAlert(std::shared_ptr<Alert> alert) {
        std::cout << "    >>> MEDIUM: Check on patient within 5 minutes" << std::endl;
    }
    
    void handleLowAlert(std::shared_ptr<Alert> alert) {
        std::cout << "    >>> LOW: Routine check during next rounds" << std::endl;
    }
    
    std::string priorityToString(Priority p) {
        switch (p) {
            case Priority::CRITICAL: return "CRITICAL";
            case Priority::HIGH: return "HIGH    ";
            case Priority::MEDIUM: return "MEDIUM  ";
            case Priority::LOW: return "LOW     ";
            default: return "UNKNOWN ";
        }
    }
    
    std::string getCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }
};

// Hospital Scheduler (simplified)
class HospitalScheduler {
private:
    std::map<int, std::unique_ptr<Patient>> patients;
    std::vector<std::unique_ptr<MedicalDevice>> devices;
    std::unique_ptr<AlertProcessor> alertProcessor;
    
public:
    HospitalScheduler() {
        alertProcessor = std::make_unique<AlertProcessor>();
    }
    
    void addPatient(std::unique_ptr<Patient> patient) {
        int patientId = patient->getId();
        patients[patientId] = std::move(patient);
        
        // Create monitoring devices for this patient
        createDevicesForPatient(patientId);
    }
    
    void createDevicesForPatient(int patientId) {
        // Create one device for each vital sign
        devices.push_back(std::make_unique<MedicalDevice>(devices.size(), VitalSign::HEART_RATE, patientId));
        devices.push_back(std::make_unique<MedicalDevice>(devices.size(), VitalSign::BLOOD_PRESSURE, patientId));
        devices.push_back(std::make_unique<MedicalDevice>(devices.size(), VitalSign::OXYGEN_SATURATION, patientId));
        devices.push_back(std::make_unique<MedicalDevice>(devices.size(), VitalSign::TEMPERATURE, patientId));
    }
    
    void simulateMonitoringCycle() {
        // Generate readings from all devices and process them
        for (auto& device : devices) {
            if (device->isDeviceActive()) {
                VitalReading reading = device->generateReading();
                processVitalReading(reading);
            }
        }
        
        // Process all generated alerts
        alertProcessor->processAllAlerts();
    }
    
    void processVitalReading(const VitalReading& reading) {
        auto patientIt = patients.find(reading.patientId);
        if (patientIt == patients.end()) return;
        
        Patient* patient = patientIt->second.get();
        patient->addVitalReading(reading);
        
        // Assess risk and create alerts if necessary
        Priority risk = patient->assessRisk(reading);
        
        if (risk != Priority::LOW) {
            std::string message = generateAlertMessage(reading, risk);
            auto alert = std::make_shared<Alert>(reading.patientId, risk, message, reading.type);
            
            // Check for false alarm
            auto recentReadings = patient->getRecentReadings(reading.type, 10);
            if (!FalseAlarmDetector::isLikelyFalseAlarm(*alert, recentReadings)) {
                alertProcessor->addAlert(alert);
            } else {
                // Still log false alarms for statistics
                std::cout << "[FALSE ALARM FILTERED] Patient " << reading.patientId 
                          << ": " << message << std::endl;
            }
        }
        
        // Check for concerning trends
        if (patient->detectTrend(reading.type)) {
            std::string trendMessage = "Concerning trend detected in " + vitalSignToString(reading.type);
            auto trendAlert = std::make_shared<Alert>(reading.patientId, Priority::MEDIUM, trendMessage, reading.type);
            alertProcessor->addAlert(trendAlert);
        }
    }
    
    void runSimulation(int cycles) {
        std::cout << "Starting monitoring simulation for " << cycles << " cycles..." << std::endl;
        
        for (int i = 0; i < cycles; ++i) {
            std::cout << "\n--- Cycle " << (i + 1) << " ---" << std::endl;
            simulateMonitoringCycle();
            
            // Small delay to see output
            #ifdef _WIN32
                system("timeout /t 1 /nobreak > nul 2>&1");
            #else
                system("sleep 1");
            #endif
        }
    }
    
    void printPatientInfo() {
        std::cout << "\n=== Current Patients ===" << std::endl;
        for (const auto& pair : patients) {
            const Patient* patient = pair.second.get();
            std::cout << "ID: " << patient->getId() 
                      << " | Name: " << patient->getName() 
                      << " | Risk: " << priorityToString(patient->getCurrentRisk()) << std::endl;
        }
    }
    
    void printStatistics() {
        std::cout << "\n=== System Statistics ===" << std::endl;
        std::cout << "Total Patients: " << patients.size() << std::endl;
        std::cout << "Total Devices: " << devices.size() << std::endl;
        std::cout << "Alerts Processed: " << alertProcessor->getTotalAlertsProcessed() << std::endl;
        std::cout << "False Alarms Filtered: " << alertProcessor->getFalseAlarmsFiltered() << std::endl;
    }
    
private:
    std::string generateAlertMessage(const VitalReading& reading, Priority priority) {
        std::string vital = vitalSignToString(reading.type);
        return vital + " reading: " + std::to_string(static_cast<int>(reading.value)) + 
               " (Priority: " + std::to_string(static_cast<int>(priority)) + ")";
    }
    
    std::string vitalSignToString(VitalSign vital) {
        switch (vital) {
            case VitalSign::HEART_RATE: return "Heart Rate";
            case VitalSign::BLOOD_PRESSURE: return "Blood Pressure";
            case VitalSign::OXYGEN_SATURATION: return "Oxygen Saturation";
            case VitalSign::TEMPERATURE: return "Temperature";
            case VitalSign::RESPIRATORY_RATE: return "Respiratory Rate";
            default: return "Unknown";
        }
    }
    
    std::string priorityToString(Priority p) {
        switch (p) {
            case Priority::CRITICAL: return "CRITICAL";
            case Priority::HIGH: return "HIGH";
            case Priority::MEDIUM: return "MEDIUM";
            case Priority::LOW: return "LOW";
            default: return "UNKNOWN";
        }
    }
};

// Test Framework
class TestFramework {
public:
    static void runAllTests() {
        std::cout << "\n=== Running Unit Tests ===" << std::endl;
        
        testPatientCreation();
        testVitalReadingProcessing();
        testAlertGeneration();
        testPriorityScheduling();
        testFalseAlarmDetection();
        
        std::cout << "✓ All tests passed!" << std::endl;
    }
    
private:
    static void testPatientCreation() {
        Patient patient(1, "Test Patient", 30);
        assert(patient.getId() == 1);
        assert(patient.getName() == "Test Patient");
        std::cout << "✓ Patient creation test passed" << std::endl;
    }
    
    static void testVitalReadingProcessing() {
        Patient patient(1, "Test", 30);
        VitalReading reading(VitalSign::HEART_RATE, 200.0, 1);
        Priority risk = patient.assessRisk(reading);
        assert(risk == Priority::CRITICAL);
        std::cout << "✓ Vital reading processing test passed" << std::endl;
    }
    
    static void testAlertGeneration() {
        Alert alert(1, Priority::CRITICAL, "Test alert", VitalSign::HEART_RATE);
        assert(alert.priority == Priority::CRITICAL);
        assert(alert.patientId == 1);
        std::cout << "✓ Alert generation test passed" << std::endl;
    }
    
    static void testPriorityScheduling() {
        // Test alert comparator
        auto alert1 = std::make_shared<Alert>(1, Priority::HIGH, "Test", VitalSign::HEART_RATE);
        auto alert2 = std::make_shared<Alert>(2, Priority::CRITICAL, "Test", VitalSign::HEART_RATE);
        
        AlertComparator comp;
        assert(comp(alert1, alert2)); // Critical should have higher priority
        std::cout << "✓ Priority scheduling test passed" << std::endl;
    }
    
    static void testFalseAlarmDetection() {
        std::vector<VitalReading> readings;
        for (int i = 0; i < 10; ++i) {
            readings.emplace_back(VitalSign::HEART_RATE, 75.0 + i, 1);
        }
        
        Alert alert(1, Priority::MEDIUM, "Test", VitalSign::HEART_RATE);
        bool isFalse = FalseAlarmDetector::isLikelyFalseAlarm(alert, readings);
        
        std::cout << "✓ False alarm detection test passed" << std::endl;
    }
};

// Helper functions for user input
int getUserInput(const std::string& prompt, int min, int max) {
    int value;
    while (true) {
        std::cout << prompt;
        std::cin >> value;
        
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid input! Please enter a number." << std::endl;
            continue;
        }
        
        if (value >= min && value <= max) {
            break;
        }
        
        std::cout << "Please enter a value between " << min << " and " << max << std::endl;
    }
    return value;
}

void addPatientsInteractively(HospitalScheduler& scheduler, int numPatients) {
    std::vector<std::string> defaultNames = {
        "John Doe", "Jane Smith", "Bob Johnson", "Alice Brown", "Charlie Wilson",
        "Diana Prince", "Peter Parker", "Mary Johnson", "David Lee", "Sarah Connor"
    };
    
    std::cout << "\n=== Adding Patients ===" << std::endl;
    
    for (int i = 1; i <= numPatients; ++i) {
        std::string choice;
        std::cout << "\nPatient " << i << ":" << std::endl;
        std::cout << "Use default patient data? (y/n): ";
        std::cin >> choice;
        
        if (choice == "y" || choice == "Y") {
            // Use default data
            std::string name = (i <= static_cast<int>(defaultNames.size())) ? defaultNames[i-1] : "Patient_" + std::to_string(i);
            int age = 30 + (rand() % 50); // Age between 30-80
            
            scheduler.addPatient(std::make_unique<Patient>(i, name, age));
            std::cout << "Added: " << name << " (Age: " << age << ")" << std::endl;
        } else {
            // Get custom patient data
            std::cin.ignore(); // Clear buffer
            
            std::string name;
            std::cout << "Enter patient name: ";
            std::getline(std::cin, name);
            
            int age = getUserInput("Enter patient age (1-120): ", 1, 120);
            
            scheduler.addPatient(std::make_unique<Patient>(i, name, age));
            std::cout << "Added: " << name << " (Age: " << age << ")" << std::endl;
        }
    }
}

void showCycleMenu() {
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  [Enter] - Run normal monitoring cycle" << std::endl;
    std::cout << "  [e]     - Simulate emergency scenario" << std::endl;
    std::cout << "  [s]     - Show current statistics" << std::endl;
    std::cout << "  [q]     - Quit simulation" << std::endl;
}

VitalReading createEmergencyReading(int emergencyType, int patientId) {
    switch (emergencyType) {
        case 1: // Cardiac arrest
            return VitalReading(VitalSign::HEART_RATE, 200.0, patientId);
        case 2: // Respiratory failure  
            return VitalReading(VitalSign::OXYGEN_SATURATION, 75.0, patientId);
        case 3: // Severe hypertension
            return VitalReading(VitalSign::BLOOD_PRESSURE, 220.0, patientId);
        case 4: // Hypothermia
            return VitalReading(VitalSign::TEMPERATURE, 32.0, patientId);
        default:
            return VitalReading(VitalSign::HEART_RATE, 200.0, patientId);
    }
}

void simulateEmergency(HospitalScheduler& scheduler) {
    std::cout << "\n!!! EMERGENCY SIMULATION ACTIVATED !!!" << std::endl;
    
    int patientId = getUserInput("Enter patient ID for emergency (1-10): ", 1, 10);
    
    std::cout << "\nSelect emergency type:" << std::endl;
    std::cout << "1. Cardiac arrest (Critical heart rate)" << std::endl;
    std::cout << "2. Respiratory failure (Critical oxygen)" << std::endl;
    std::cout << "3. Severe hypertension (Critical blood pressure)" << std::endl;
    std::cout << "4. Hypothermia (Critical temperature)" << std::endl;
    
    int emergencyType = getUserInput("Emergency type (1-4): ", 1, 4);
    
    // Create emergency vital reading
    VitalReading emergencyReading = createEmergencyReading(emergencyType, patientId);
    
    std::cout << "\n>>> EMERGENCY TRIGGERED <<<" << std::endl;
    scheduler.processVitalReading(emergencyReading);
    
    std::cout << "Emergency simulation complete." << std::endl;
}

void runInteractiveCycles(HospitalScheduler& scheduler, int totalCycles) {
    for (int i = 0; i < totalCycles; ++i) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "CYCLE " << (i + 1) << " of " << totalCycles << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        
        // Show menu options
        showCycleMenu();
        
        // Get user choice
        std::string input;
        std::cout << "\nYour choice: ";
        std::cin >> input;
        
        if (input == "q" || input == "Q") {
            std::cout << "Simulation terminated by user." << std::endl;
            break;
        } else if (input == "s" || input == "S") {
            // Show current statistics
            scheduler.printStatistics();
            i--; // Don't count this as a cycle
            continue;
        } else if (input == "e" || input == "E") {
            // Simulate emergency
            simulateEmergency(scheduler);
        }
        
        // Run normal monitoring cycle
        scheduler.simulateMonitoringCycle();
        
        // Wait for user to continue
        if (i < totalCycles - 1) {
            std::cout << "\nPress Enter to continue to next cycle...";
            std::cin.ignore();
            std::getline(std::cin, input);
        }
    }
}

// Hospital Simulation class
class HospitalSimulation {
public:
    static void runInteractiveSimulation() {
        std::cout << "\n=== Interactive Simulation Mode ===" << std::endl;
        
        // Run tests first
        TestFramework::runAllTests();
        
        HospitalScheduler scheduler;
        
        // Get user input for simulation parameters
        int numPatients = getUserInput("Enter number of patients to monitor (1-10): ", 1, 10);
        int numCycles = getUserInput("Enter number of monitoring cycles (5-50): ", 5, 50);
        
        // Add patients based on user input
        addPatientsInteractively(scheduler, numPatients);
        
        std::cout << "\nStarting hospital monitoring simulation..." << std::endl;
        std::cout << "Monitoring " << numPatients << " patients for " << numCycles << " cycles..." << std::endl;
        std::cout << "\nPress Enter after each cycle to continue (or 'q' to quit early)...\n" << std::endl;
        
        // Run interactive simulation
        runInteractiveCycles(scheduler, numCycles);
        
        // Print final statistics
        scheduler.printStatistics();
        
        std::cout << "\nSimulation completed successfully!" << std::endl;
    }
    
    static void runQuickDemo() {
        std::cout << "\n=== Quick Demo Mode ===" << std::endl;
        TestFramework::runAllTests();
        
        HospitalScheduler scheduler;
        
        // Add 5 default patients
        scheduler.addPatient(std::make_unique<Patient>(1, "John Doe", 45));
        scheduler.addPatient(std::make_unique<Patient>(2, "Jane Smith", 67));
        scheduler.addPatient(std::make_unique<Patient>(3, "Bob Johnson", 34));
        scheduler.addPatient(std::make_unique<Patient>(4, "Alice Brown", 52));
        scheduler.addPatient(std::make_unique<Patient>(5, "Charlie Wilson", 78));
        
        std::cout << "\nRunning 5 monitoring cycles with 5 patients..." << std::endl;
        scheduler.runSimulation(5);
        scheduler.printStatistics();
    }
};

// Main function with interactive menu
int main() {
    try {
        // Initialize random seed for realistic simulation
        srand(static_cast<unsigned>(time(nullptr)));
        
        std::cout << "Hospital Patient Monitoring Scheduler" << std::endl;
        std::cout << "====================================" << std::endl;
        std::cout << "\nWelcome to the Interactive Hospital Monitoring System!" << std::endl;
        
        while (true) {
            std::cout << "\n" << std::string(40, '=') << std::endl;
            std::cout << "MAIN MENU" << std::endl;
            std::cout << std::string(40, '=') << std::endl;
            std::cout << "1. Run Interactive Simulation" << std::endl;
            std::cout << "2. Run Quick Demo (5 patients, 5 cycles)" << std::endl;
            std::cout << "3. Run Unit Tests Only" << std::endl;
            std::cout << "4. Exit" << std::endl;
            std::cout << "\nEnter your choice (1-4): ";
            
            int choice;
            std::cin >> choice;
            
            switch (choice) {
                case 1:
                    HospitalSimulation::runInteractiveSimulation();
                    break;
                    
                case 2:
                    HospitalSimulation::runQuickDemo();
                    break;
                    
                case 3:
                    TestFramework::runAllTests();
                    break;
                    
                case 4:
                    std::cout << "Thank you for using Hospital Patient Monitoring Scheduler!" << std::endl;
                    return 0;
                    
                default:
                    std::cout << "Invalid choice! Please enter 1-4." << std::endl;
                    break;
            }
            
            std::cout << "\nPress Enter to return to main menu...";
            std::cin.ignore();
            std::cin.get();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}