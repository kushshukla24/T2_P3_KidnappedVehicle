/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

static default_random_engine gen;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// TODO: Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.
	// NOTE: Consult particle_filter.h for more information about this method (and others in this file).

	num_particles = 100;
	
	normal_distribution<double> dist_x(x, std[0]);
	normal_distribution<double> dist_y(y, std[1]);
	normal_distribution<double> dist_theta(theta, std[2]);

	for( int i =0; i<num_particles;++i) {
		Particle p;
		p.weight = 1.;
		p.id = i;
		p.x = dist_x(gen);
		p.y = dist_y(gen);
		p.theta = dist_theta(gen);
		particles.push_back(p);
	}
	is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// TODO: Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	for (auto &particle : particles) {

		if (abs(yaw_rate) < numeric_limits<double>::epsilon()) {
			particle.x += velocity * cos(particle.theta) * delta_t;
			particle.y += velocity * sin(particle.theta) * delta_t;
		}
		else {
			particle.x += (velocity / yaw_rate)*(sin(particle.theta + (yaw_rate*delta_t)) - sin(particle.theta));
			particle.y += (velocity / yaw_rate)*(cos(particle.theta) - cos(particle.theta + (yaw_rate*delta_t)));
			particle.theta += yaw_rate*delta_t;
		}

		normal_distribution<double> dist_x(particle.x, std_pos[0]);
		normal_distribution<double> dist_y(particle.y, std_pos[1]);
		normal_distribution<double> dist_theta(particle.theta, std_pos[2]);

		particle.x = dist_x(gen);
		particle.y = dist_y(gen);
		particle.theta = dist_theta(gen);
	}
}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// TODO: Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.

	for (auto &observation : observations) {
		double minDistance = numeric_limits<double>::max();
		for (const auto &measurement : predicted) {
			double distance = dist(observation.x, observation.y, measurement.x, measurement.y);
			if (distance  < minDistance) {
				minDistance = distance;
				observation.id = measurement.id;
			}
		}
	}
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// TODO: Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	for (auto &particle : particles) 
	{
		vector<LandmarkObs> predictedLandmarks;
		for (auto &mapLandmark : map_landmarks.landmark_list) 
		{
			double particleLandmarkDistance = dist(particle.x, particle.y, 
													static_cast<double>(mapLandmark.x_f), static_cast<double>(mapLandmark.y_f));

			if ((particleLandmarkDistance - sensor_range) < numeric_limits<double>::epsilon())
				predictedLandmarks.push_back(
					LandmarkObs({id:mapLandmark.id_i, x:static_cast<double>(mapLandmark.x_f), y: static_cast<double>(mapLandmark.y_f)}));
		}

		vector<LandmarkObs> observedLandmarks;
		for (const auto &observation : observations) 
		{
			LandmarkObs landmark;
			landmark.x = cos(particle.theta) * observation.x - sin(particle.theta) * observation.y + particle.x;
			landmark.y = sin(particle.theta) * observation.x + cos(particle.theta) * observation.y + particle.y;
			observedLandmarks.push_back(landmark);
		}

		dataAssociation(predictedLandmarks, observedLandmarks);

		double probabilityParticle = 1., muX = 0., muY = 0.;
		for (const auto &observation : observedLandmarks) {
			for (const auto &predictedLandmark : predictedLandmarks) {
				if (observation.id == predictedLandmark.id) {
					muX = predictedLandmark.x;
					muY = predictedLandmark.y;
				}
			}
			double probDenominator = 2 * M_PI * std_landmark[0] * std_landmark[1];
			double probNumerator = exp(-(pow(observation.x - muX, 2) / (2 * pow(std_landmark[0],2)) 
								+ pow(observation.y - muY, 2) / (2 * pow(std_landmark[1], 2))));

			probabilityParticle *= (probNumerator / probDenominator);
		}

		particle.weight = probabilityParticle;
	}
	
	// Weight Normalization
	double sumWeights = 0.;
	for (const auto &particle : particles)
		sumWeights += particle.weight;

	if (sumWeights > numeric_limits<double>::epsilon()) {
		for (auto &particle : particles)
			particle.weight /= sumWeights;
	}
}

void ParticleFilter::resample() {
	// TODO: Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	vector<double> particleWeights;
	for (const auto &particle : particles)
		particleWeights.push_back(particle.weight);

	discrete_distribution<int> weightedDistribution(particleWeights.begin(), particleWeights.end());

	vector<Particle> resampledParticles;
	for (int i = 0;i < num_particles;++i)
		resampledParticles.push_back(particles[weightedDistribution(gen)]);

	particles = resampledParticles;
}

Particle ParticleFilter::SetAssociations(Particle& particle, const std::vector<int>& associations, 
                                     const std::vector<double>& sense_x, const std::vector<double>& sense_y)
{
    //particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
    // associations: The landmark id that goes along with each listed association
    // sense_x: the associations x mapping already converted to world coordinates
    // sense_y: the associations y mapping already converted to world coordinates

    particle.associations= associations;
    particle.sense_x = sense_x;
    particle.sense_y = sense_y;

		return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
