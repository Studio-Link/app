import Shepherd from 'shepherd.js';

const tour = new Shepherd.Tour({
	defaultStepOptions: {
		classes: 'shadow-md bg-purple-dark',
		scrollTo: true,

	},
});
tour.addStep({
	id: 'audio-interface-step',
	text: 'Select your Audio Interface',
	attachTo: {
		element: '#btn-interface',
		on: 'bottom',
		 offset: '100px',
	},
	tetherOptions:{
		offset: '-15px 0'
	},
	buttons: [
		{
			text: 'Skip Tour',
			action: tour.cancel,
			classes: 'shepherd-button-secondary',
		},

		{
			text: 'Next',
			action: tour.next
		}
	]
});
tour.addStep({
	id: 'test-call-step',
	text: 'Enter <b>echo</b> and press <b>Call</b>',
	attachTo: {
		element: '#sipnumbercall',
		on: 'left'
	},
	tetherOptions:{
		offset: '0 15px'
	},
	buttons: [
		{
			text: 'Skip Tour',
			action: tour.cancel,
			classes: 'shepherd-button-secondary',
		},

		{
			text: 'Next',
			action: tour.next
		}
	]
});

tour.on('cancel', function() {
	localStorage.setItem('slonboarding', 'completed');
});

tour.on('complete', function() {
	localStorage.setItem('slonboarding', 'completed');
});

$(function () {
	var onboarding = localStorage.getItem('slonboarding');
	if (onboarding != 'completed' ) {
		tour.start();
	}
});
