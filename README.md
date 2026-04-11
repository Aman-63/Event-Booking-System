Event Ticket Booking System (Web-Based)
Object-Oriented Programming Project
Department of Computer Science & Engineering | NIT Silchar
---
1. Objective
This project aims to design and implement an Event Ticket Booking System using C++ and Object-Oriented Programming principles, enhanced with a web-based user interface. The system allows users to view available events, book tickets, and manage cancellations while focusing on transitioning from a traditional console-based interface to a more modern web application interface. The primary goal is to apply OOP concepts to improve the user experience and system modularity.
2. System Design Overview
The system is structured around multiple classes that manage the logic of event scheduling and user bookings. Below is a summary of the key classes and their roles:
Class	Role
BookingSystem	Main controller. Handles core operations like processing bookings and managing event data.
User	Stores customer details, authentication status (conceptual), and individual booking history.
Event	Base class for event data. Stores name, date, venue, price, and seat availability.
MovieEvent / ConcertEvent	Derived classes. Provides specialized behavior and attributes for specific event types.
Ticket	Represents the final booking information generated after a successful transaction.
Web Interface Layer	Handles the UI interaction, bridging the C++ logic with HTML/CSS/JS elements.
3. Working of the System
The following steps describe the operational flow of the system from user interaction to ticket generation:
1.	The user interacts with the web interface to view available listings.
2.	The system displays a list of active events retrieved from the Event classes.
3.	The user selects a specific event and enters their personal and booking details.
4.	The BookingSystem processes the request, validating seat availability and input data.
5.	A Ticket object is generated, confirming the reservation for the user.
6.	If a user cancels, the system updates seat availability and booking logs accordingly.
7.	Error handling ensures that invalid inputs or overbooking scenarios are managed via exceptions.
4. OOP Concepts Demonstrated
Concept	How It Is Applied
Encapsulation	Event details and user booking history are kept as private members, accessed through public getters and setters.
Abstraction	Users interact with the booking interface without needing to understand the underlying seat-allocation logic or data management.
Inheritance	MovieEvent and ConcertEvent inherit core attributes from the base Event class.
Polymorphism	Used to handle different event types through a common interface, allowing the system to process various event bookings dynamically.
Composition	The BookingSystem is composed of Event and User objects to manage the overall flow of the application.
5. Key Features
•	Comprehensive event listing and ticket booking functionality.
•	Integrated cancellation system with automatic seat updates.
•	Web-based UI using HTML, CSS, and JavaScript for improved user experience.
•	Robust input validation and exception handling for system stability.
•	Modular and scalable design utilizing C++ and the Standard Template Library (STL).
6. Limitations
•	The project is currently 60% complete, with backend integration still in progress.
•	There is no persistent database; data storage is currently handled in-memory.
•	Lacks advanced security features such as user authentication and payment gateway integration.
•	The web UI is currently limited in scope and requires further refinement.
7. Conclusion
This project demonstrates the real-world application of Object-Oriented Programming by evolving a standard booking logic into a functional web-based system. By prioritizing modular design and clear class hierarchies, the system provides a scalable foundation for future enhancements like database integration and secure authentication.
“This project focuses on applying OOP principles to bridge the gap between core C++ logic and modern web-based interfaces, demonstrating a modular approach to real-world system design.”
## updated by shreya markam
