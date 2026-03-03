from abc import ABC, abstractmethod
import logging

class State(ABC):
    """
    Abstract base class for a state in the state machine.
    """
    def __init__(self, name):
        self.name = name
        self.logger = logging.getLogger(self.name)

    @abstractmethod
    def enter(self):
        """
        Called when entering the state.
        """
        pass

    @abstractmethod
    def exit(self):
        """
        Called when exiting the state.
        """
        pass

    @abstractmethod
    def update(self):
        """
        Called while within the state to perform any necessary actions.
        """
        pass

    @abstractmethod
    def check_transitions(self):
        """
        Checks for conditions to transition to another state.
        Returns the next State object or None if no transition.
        """
        pass

class StateMachine:
    """
    Manages the current state and transitions.
    """
    def __init__(self, initial_state: State = None):
        self.current_state = initial_state
        if self.current_state:
            self.current_state.enter()

    def change_state(self, new_state: State):
        """
        Transitions from the current state to a new state.
        """
        if self.current_state:
            self.current_state.exit()
        
        self.current_state = new_state
        
        if self.current_state:
            self.current_state.enter()

    def update(self):
        """
        Updates the current state and checks for transitions.
        """
        if self.current_state:
            self.current_state.update()
            next_state = self.current_state.check_transitions()
            if next_state:
                self.change_state(next_state)
