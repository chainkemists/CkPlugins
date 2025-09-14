import type { PipelineConfigInterface } from 'jsr:@runreal/buildkite-ts'

export const config: PipelineConfigInterface = {
	env: {
		RUNREAL_VERSION: '1.9.0',
		RUNREAL_PROJECT_PATH: './',
		RUNREAL_ENGINE_PATH: '$BUILDKITE_BUILD_PATH/UnrealEngine',
		RUNREAL_BUILD_PATH: './.runreal/build',
		BUILDKITE_TS_DIR: './.runreal',
	},
	plugins: {
		runreal: {
			'runreal/runreal': {
				version: '$RUNREAL_VERSION',
				from_source: '${RUNREAL_FROM_SOURCE:-false}',
				from_ref: '${RUNREAL_FROM_REF:-}',
			},
		},
		clean: {
			'git-clean#v1.0.0': {
				flags:
					'-fdx --exclude=*Intermediate/ --exclude=*Binaries/ --exclude=Saved/ --exclude=.runreal/build/',
			},
		},
	},
}
