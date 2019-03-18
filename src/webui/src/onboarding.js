import Shepherd from 'shepherd.js';

const tour = new Shepherd.Tour({
	defaultStepOptions: {
		classes: 'shadow-md bg-purple-dark',
		scrollTo: true,
		tippyOptions: {
			zIndex: 10
		}
	}
});
tour.addStep('audio-interface-step', {
	text: 'Select your Audio Interface',
	attachTo: '#btn-interface bottom',
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
tour.addStep('test-call-step', {
	text: 'Enter <b>echo@studio-link.de</b> and press <b>Call</b>',
	attachTo: '#sipnumbercall left',
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

tour.addStep('stereo-call-step', {
	text: 'If you hear your own voice only on one side, select <b>Mono</b>',
	attachTo: '#btn-mono bottom',
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

$(function () {
	tour.start();
});
